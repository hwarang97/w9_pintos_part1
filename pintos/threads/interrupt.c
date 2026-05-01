#include "threads/interrupt.h"
#include <debug.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include "threads/flags.h"
#include "threads/intr-stubs.h"
#include "threads/io.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/gdt.h"
#endif

/* x86_64 interrupt 개수. */
#define INTR_CNT 256

/* FUNCTION을 호출하는 gate를 만든다.

   gate는 descriptor privilege level DPL을 가진다. 이는 processor가 DPL
   또는 더 낮은 번호의 ring에 있을 때 의도적으로 호출될 수 있음을 뜻한다.
   실제로 DPL==3은 user mode가 gate를 호출할 수 있게 하고, DPL==0은 그런
   호출을 막는다. user mode에서 발생한 fault와 exception은 여전히 DPL==0인
   gate가 호출되게 한다.

   TYPE은 interrupt gate용 14 또는 trap gate용 15여야 한다. 차이는 interrupt
   gate에 진입하면 interrupt가 비활성화되지만 trap gate에 진입하면 그렇지
   않다는 점이다. 자세한 논의는 [IA32-v3a] 5.12.1.2 "Flag Usage By
   Exception- or Interrupt-Handler Procedure"를 참고한다. */

struct gate {
	unsigned off_15_0 : 16;   // segment 안 offset의 하위 16 bit
	unsigned ss : 16;         // segment selector
	unsigned ist : 3;        // 인자 수, interrupt/trap gate에서는 0
	unsigned rsv1 : 5;        // reserved, 0이어야 한다
	unsigned type : 4;        // type(STS_{TG,IG32,TG32})
	unsigned s : 1;           // 반드시 0(system)
	unsigned dpl : 2;         // descriptor privilege level
	unsigned p : 1;           // present
	unsigned off_31_16 : 16;  // segment 안 offset의 상위 bit
	uint32_t off_32_63;
	uint32_t rsv2;
};

/* interrupt descriptor table(IDT). 형식은 CPU가 고정한다.
   [IA32-v3a] 5.10 "Interrupt Descriptor Table (IDT)", 5.11
   "IDT Descriptors", 5.12.1.2 "Flag Usage By Exception- or
   Interrupt-Handler Procedure"를 참고한다. */
static struct gate idt[INTR_CNT];

static struct desc_ptr idt_desc = {
	.size = sizeof(idt) - 1,
	.address = (uint64_t) idt
};


#define make_gate(g, function, d, t) \
{ \
	ASSERT ((function) != NULL); \
	ASSERT ((d) >= 0 && (d) <= 3); \
	ASSERT ((t) >= 0 && (t) <= 15); \
	*(g) = (struct gate) { \
		.off_15_0 = (uint64_t) (function) & 0xffff, \
		.ss = SEL_KCSEG, \
		.ist = 0, \
		.rsv1 = 0, \
		.type = (t), \
		.s = 0, \
		.dpl = (d), \
		.p = 1, \
		.off_31_16 = ((uint64_t) (function) >> 16) & 0xffff, \
		.off_32_63 = ((uint64_t) (function) >> 32) & 0xffffffff, \
		.rsv2 = 0, \
	}; \
}

/* 주어진 DPL로 FUNCTION을 호출하는 interrupt gate를 만든다. */
#define make_intr_gate(g, function, dpl) make_gate((g), (function), (dpl), 14)

/* 주어진 DPL로 FUNCTION을 호출하는 trap gate를 만든다. */
#define make_trap_gate(g, function, dpl) make_gate((g), (function), (dpl), 15)



/* 각 interrupt에 대한 interrupt handler 함수. */
static intr_handler_func *intr_handlers[INTR_CNT];

/* debugging용 각 interrupt 이름. */
static const char *intr_names[INTR_CNT];

/* external interrupt는 timer처럼 CPU 밖의 device가 생성하는 interrupt다.
   external interrupt는 interrupt가 꺼진 상태로 실행되므로 중첩되지 않고
   preempt되지도 않는다. external interrupt handler는 sleep할 수 없지만,
   interrupt가 반환되기 직전에 새 process가 schedule되도록 intr_yield_on_return()
   을 호출할 수는 있다. */
static bool in_external_intr;   /* external interrupt 처리 중인가? */
static bool yield_on_return;    /* interrupt return 시 yield해야 하는가? */

/* programmable interrupt controller 보조 함수. */
static void pic_init (void);
static void pic_end_of_interrupt (int irq);

/* interrupt handler. */
void intr_handler (struct intr_frame *args);

/* 현재 interrupt 상태를 반환한다. */
enum intr_level
intr_get_level (void) {
	uint64_t flags;

	/* processor stack에 flags register를 push한 뒤 stack에서 값을 pop해
	   `flags'에 넣는다. [IA32-v2b] "PUSHF"와 "POP", [IA32-v3a] 5.8.1
	   "Masking Maskable Hardware Interrupts"를 참고한다. */
	asm volatile ("pushfq; popq %0" : "=g" (flags));

	return flags & FLAG_IF ? INTR_ON : INTR_OFF;
}

/* LEVEL이 지정한 대로 interrupt를 활성화하거나 비활성화하고 이전 interrupt
   상태를 반환한다. */
enum intr_level
intr_set_level (enum intr_level level) {
	return level == INTR_ON ? intr_enable () : intr_disable ();
}

/* interrupt를 활성화하고 이전 interrupt 상태를 반환한다. */
enum intr_level
intr_enable (void) {
	enum intr_level old_level = intr_get_level ();
	ASSERT (!intr_context ());

	/* interrupt flag를 설정해 interrupt를 활성화한다.

	   [IA32-v2b] "STI"와 [IA32-v3a] 5.8.1 "Masking Maskable Hardware
	   Interrupts"를 참고한다. */
	asm volatile ("sti");

	return old_level;
}

/* interrupt를 비활성화하고 이전 interrupt 상태를 반환한다. */
enum intr_level
intr_disable (void) {
	enum intr_level old_level = intr_get_level ();

	/* interrupt flag를 clear해 interrupt를 비활성화한다.
	   [IA32-v2b] "CLI"와 [IA32-v3a] 5.8.1 "Masking Maskable Hardware
	   Interrupts"를 참고한다. */
	asm volatile ("cli" : : : "memory");

	return old_level;
}

/* interrupt system을 초기화한다. */
void
intr_init (void) {
	int i;

	/* interrupt controller를 초기화한다. */
	pic_init ();

	/* IDT를 초기화한다. */
	for (i = 0; i < INTR_CNT; i++) {
		make_intr_gate(&idt[i], intr_stubs[i], 0);
		intr_names[i] = "unknown";
	}

#ifdef USERPROG
	/* TSS를 load한다. */
	ltr (SEL_TSS);
#endif

	/* IDT register를 load한다. */
	lidt(&idt_desc);

	/* intr_names를 초기화한다. */
	intr_names[0] = "#DE Divide Error";
	intr_names[1] = "#DB Debug Exception";
	intr_names[2] = "NMI Interrupt";
	intr_names[3] = "#BP Breakpoint Exception";
	intr_names[4] = "#OF Overflow Exception";
	intr_names[5] = "#BR BOUND Range Exceeded Exception";
	intr_names[6] = "#UD Invalid Opcode Exception";
	intr_names[7] = "#NM Device Not Available Exception";
	intr_names[8] = "#DF Double Fault Exception";
	intr_names[9] = "Coprocessor Segment Overrun";
	intr_names[10] = "#TS Invalid TSS Exception";
	intr_names[11] = "#NP Segment Not Present";
	intr_names[12] = "#SS Stack Fault Exception";
	intr_names[13] = "#GP General Protection Exception";
	intr_names[14] = "#PF Page-Fault Exception";
	intr_names[16] = "#MF x87 FPU Floating-Point Error";
	intr_names[17] = "#AC Alignment Check Exception";
	intr_names[18] = "#MC Machine-Check Exception";
	intr_names[19] = "#XF SIMD Floating-Point Exception";
}

/* interrupt VEC_NO를 등록해 descriptor privilege level DPL로 HANDLER를
   호출하게 한다. debugging을 위해 interrupt 이름을 NAME으로 지정한다.
   interrupt handler는 interrupt 상태가 LEVEL로 설정된 상태에서 호출된다. */
static void
register_handler (uint8_t vec_no, int dpl, enum intr_level level,
		intr_handler_func *handler, const char *name) {
	ASSERT (intr_handlers[vec_no] == NULL);
	if (level == INTR_ON) {
		make_trap_gate(&idt[vec_no], intr_stubs[vec_no], dpl);
	}
	else {
		make_intr_gate(&idt[vec_no], intr_stubs[vec_no], dpl);
	}
	intr_handlers[vec_no] = handler;
	intr_names[vec_no] = name;
}

/* external interrupt VEC_NO를 등록해 HANDLER를 호출하게 한다. debugging을 위해
   이름은 NAME으로 지정한다. handler는 interrupt가 비활성화된 상태에서 실행된다. */
void
intr_register_ext (uint8_t vec_no, intr_handler_func *handler,
		const char *name) {
	ASSERT (vec_no >= 0x20 && vec_no <= 0x2f);
	register_handler (vec_no, 0, INTR_OFF, handler, name);
}

/* internal interrupt VEC_NO를 등록해 HANDLER를 호출하게 한다. debugging을 위해
   이름은 NAME으로 지정한다. interrupt handler는 interrupt 상태 LEVEL로 호출된다.

   handler는 descriptor privilege level DPL을 가진다. 이는 processor가 DPL
   또는 더 낮은 번호의 ring에 있을 때 의도적으로 호출될 수 있음을 뜻한다.
   실제로 DPL==3은 user mode가 interrupt를 호출할 수 있게 하고, DPL==0은
   그런 호출을 막는다. user mode에서 발생한 fault와 exception은 여전히
   DPL==0 interrupt가 호출되게 한다. 자세한 내용은 [IA32-v3a] 4.5
   "Privilege Levels"와 4.8.1.1 "Accessing Nonconforming Code Segments"를
   참고한다. */
void
intr_register_int (uint8_t vec_no, int dpl, enum intr_level level,
		intr_handler_func *handler, const char *name)
{
	ASSERT (vec_no < 0x20 || vec_no > 0x2f);
	register_handler (vec_no, dpl, level, handler, name);
}

/* external interrupt 처리 중이면 true, 그 외에는 false를 반환한다. */
bool
intr_context (void) {
	return in_external_intr;
}

/* external interrupt 처리 중 interrupt에서 반환하기 직전에 새 process로
   yield하도록 interrupt handler에 지시한다. 다른 시점에는 호출할 수 없다. */
void
intr_yield_on_return (void) {
	ASSERT (intr_context ());
	yield_on_return = true;
}

/* 8259A Programmable Interrupt Controller. */

/* 모든 PC에는 8259A Programmable Interrupt Controller(PIC) chip이 두 개 있다.
   하나는 port 0x20과 0x21에서 접근 가능한 "master"다. 다른 하나는 master의
   IRQ 2 line에 cascade된 "slave"이며 port 0xa0과 0xa1에서 접근 가능하다.
   port 0x20 접근은 A0 line을 0으로 설정하고, 0x21 접근은 A1 line을 1로
   설정한다. slave PIC도 비슷하다.

   기본적으로 PIC가 전달하는 interrupt 0...15는 interrupt vector 0...15로 간다.
   그런데 그 vector들은 CPU trap과 exception에도 사용된다. 그래서 PIC를 다시
   programming해 interrupt 0...15가 대신 interrupt vector 32...47(0x20...0x2f)로
   전달되도록 한다. */

/* PIC를 초기화한다. 자세한 내용은 [8259A]를 참고한다. */
static void
pic_init (void) {
	/* 두 PIC의 모든 interrupt를 mask한다. */
	outb (0x21, 0xff);
	outb (0xa1, 0xff);

	/* master를 초기화한다. */
	outb (0x20, 0x11); /* ICW1: single mode, edge triggered, expect ICW4. */
	outb (0x21, 0x20); /* ICW2: line IR0...7 -> irq 0x20...0x27. */
	outb (0x21, 0x04); /* ICW3: slave PIC on line IR2. */
	outb (0x21, 0x01); /* ICW4: 8086 mode, normal EOI, non-buffered. */

	/* slave를 초기화한다. */
	outb (0xa0, 0x11); /* ICW1: single mode, edge triggered, expect ICW4. */
	outb (0xa1, 0x28); /* ICW2: line IR0...7 -> irq 0x28...0x2f. */
	outb (0xa1, 0x02); /* ICW3: slave ID is 2. */
	outb (0xa1, 0x01); /* ICW4: 8086 mode, normal EOI, non-buffered. */

	/* 모든 interrupt의 mask를 해제한다. */
	outb (0x21, 0x00);
	outb (0xa1, 0x00);
}

/* 주어진 IRQ에 대해 PIC에 end-of-interrupt signal을 보낸다.
   IRQ를 acknowledge하지 않으면 다시 전달되지 않으므로 중요하다. */
static void
pic_end_of_interrupt (int irq) {
	ASSERT (irq >= 0x20 && irq < 0x30);

	/* master PIC를 acknowledge한다. */
	outb (0x20, 0x20);

	/* slave interrupt라면 slave PIC를 acknowledge한다. */
	if (irq >= 0x28)
		outb (0xa0, 0x20);
}
/* interrupt handler. */

/* 모든 interrupt, fault, exception을 위한 handler. 이 함수는 intr-stubs.S의
   assembly interrupt stub이 호출한다. FRAME은 interrupt와 interrupt된 thread의
   register를 설명한다. */
void
intr_handler (struct intr_frame *frame) {
	bool external;
	intr_handler_func *handler;

	/* external interrupt는 특별하다. 한 번에 하나만 처리하므로 interrupt는 꺼져
	   있어야 하고, PIC에서 acknowledge되어야 한다(아래 참고).
	   external interrupt handler는 sleep할 수 없다. */
	external = frame->vec_no >= 0x20 && frame->vec_no < 0x30;
	if (external) {
		ASSERT (intr_get_level () == INTR_OFF);
		ASSERT (!intr_context ());

		in_external_intr = true;
		yield_on_return = false;
	}

	/* interrupt의 handler를 호출한다. */
	handler = intr_handlers[frame->vec_no];
	if (handler != NULL)
		handler (frame);
	else if (frame->vec_no == 0x27 || frame->vec_no == 0x2f) {
		/* handler는 없지만 이 interrupt는 hardware fault나 hardware race condition
		   때문에 spurious하게 발생할 수 있다. 무시한다. */
	} else {
		/* handler도 없고 spurious도 아니다. unexpected interrupt handler를 호출한다. */
		intr_dump_frame (frame);
		PANIC ("Unexpected interrupt");
	}

	/* external interrupt 처리를 완료한다. */
	if (external) {
		ASSERT (intr_get_level () == INTR_OFF);
		ASSERT (intr_context ());

		in_external_intr = false;
		pic_end_of_interrupt (frame->vec_no);

		if (yield_on_return)
			thread_yield ();
	}
}

/* debugging을 위해 interrupt frame F를 console에 dump한다. */
void
intr_dump_frame (const struct intr_frame *f) {
	/* CR2는 마지막 page fault의 linear address다.
	   [IA32-v2a] "MOV--Move to/from Control Registers"와 [IA32-v3a] 5.14
	   "Interrupt 14--Page Fault Exception (#PF)"를 참고한다. */
	uint64_t cr2 = rcr2();
	printf ("Interrupt %#04llx (%s) at rip=%llx\n",
			f->vec_no, intr_names[f->vec_no], f->rip);
	printf (" cr2=%016llx error=%16llx\n", cr2, f->error_code);
	printf ("rax %016llx rbx %016llx rcx %016llx rdx %016llx\n",
			f->R.rax, f->R.rbx, f->R.rcx, f->R.rdx);
	printf ("rsp %016llx rbp %016llx rsi %016llx rdi %016llx\n",
			f->rsp, f->R.rbp, f->R.rsi, f->R.rdi);
	printf ("rip %016llx r8 %016llx  r9 %016llx r10 %016llx\n",
			f->rip, f->R.r8, f->R.r9, f->R.r10);
	printf ("r11 %016llx r12 %016llx r13 %016llx r14 %016llx\n",
			f->R.r11, f->R.r12, f->R.r13, f->R.r14);
	printf ("r15 %016llx rflags %08llx\n", f->R.r15, f->eflags);
	printf ("es: %04x ds: %04x cs: %04x ss: %04x\n",
			f->es, f->ds, f->cs, f->ss);
}

/* interrupt VEC의 이름을 반환한다. */
const char *
intr_name (uint8_t vec) {
	return intr_names[vec];
}
