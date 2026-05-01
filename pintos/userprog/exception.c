#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "intrinsic.h"

/* 처리한 page fault의 수. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* 유저 프로그램이 일으킬 수 있는 interrupt의 handler를 등록한다.

   실제 Unix 계열 OS에서는 대부분의 interrupt가 [SV-386] 3-24와
   3-25에 설명된 signal 형태로 유저 프로세스에 전달되지만,
   여기서는 signal을 구현하지 않는다. 대신 단순히 유저 프로세스를
   종료하도록 만든다.

   page fault는 예외다. 여기서는 다른 예외와 같은 방식으로 처리하지만,
   virtual memory를 구현하려면 이 부분은 바뀌어야 한다.

   각 예외에 대한 설명은 [IA32-v3a] section 5.15
   "Exception and Interrupt Reference"를 참고한다. */
void
exception_init (void) {
	/* 이 예외들은 INT, INT3, INTO, BOUND 명령어 등을 통해
	   유저 프로그램이 명시적으로 발생시킬 수 있다.
	   따라서 DPL==3으로 설정한다. 이는 유저 프로그램이 해당 명령어로
	   이 예외들을 호출할 수 있다는 뜻이다. */
	intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
	intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
	intr_register_int (5, 3, INTR_ON, kill,
			"#BR BOUND Range Exceeded Exception");

	/* 이 예외들은 DPL==0을 가진다. 따라서 유저 프로세스가 INT 명령어로
	   직접 호출할 수 없다. 하지만 간접적으로 발생할 수는 있다.
	   예를 들어 #DE는 0으로 나누면 발생할 수 있다. */
	intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
	intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
	intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
	intr_register_int (7, 0, INTR_ON, kill,
			"#NM Device Not Available Exception");
	intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
	intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
	intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
	intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
	intr_register_int (19, 0, INTR_ON, kill,
			"#XF SIMD Floating-Point Exception");

	/* 대부분의 예외는 interrupt를 켠 상태로 처리할 수 있다.
	   page fault는 fault address가 CR2에 저장되고 보존되어야 하므로
	   interrupt를 끈 상태로 처리해야 한다. */
	intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* 예외 통계를 출력한다. */
void
exception_print_stats (void) {
	printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* 유저 프로세스가 일으켰을 가능성이 높은 예외의 handler. */
static void
kill (struct intr_frame *f) {
	/* 이 interrupt는 유저 프로세스가 일으켰을 가능성이 높다.
	   예를 들어 프로세스가 매핑되지 않은 가상 메모리에 접근하려 했을 수 있다.
	   현재는 단순히 유저 프로세스를 종료한다. 나중에는 page fault를
	   커널에서 처리해야 한다. 실제 Unix 계열 운영체제는 대부분의 예외를
	   signal을 통해 프로세스에 다시 전달하지만, 여기서는 구현하지 않는다. */

	/* interrupt frame의 code segment 값은 예외가 어디서 발생했는지 알려준다. */
	switch (f->cs) {
		case SEL_UCSEG:
			/* 유저 code segment이므로 예상대로 유저 예외다.
			   유저 프로세스를 종료한다. */
			printf ("%s: dying due to interrupt %#04llx (%s).\n",
					thread_name (), f->vec_no, intr_name (f->vec_no));
			intr_dump_frame (f);
			thread_exit ();

		case SEL_KCSEG:
			/* 커널 code segment이며, 이는 커널 버그를 의미한다.
			   커널 코드는 예외를 던지면 안 된다. page fault가 커널 예외를
			   일으킬 수는 있지만 여기로 도착해서는 안 된다.
			   문제를 분명히 하기 위해 커널 panic을 발생시킨다. */
			intr_dump_frame (f);
			PANIC ("Kernel bug - unexpected interrupt in kernel");

		default:
			/* 다른 code segment라면 발생하면 안 되는 상황이다.
			   커널 panic을 발생시킨다. */
			printf ("Interrupt %#04llx (%s) in unknown segment %04x\n",
					f->vec_no, intr_name (f->vec_no), f->cs);
			thread_exit ();
	}
}

/* page fault handler. virtual memory를 구현하려면 채워야 하는 뼈대 코드다.
   project 2의 일부 해법에서도 이 코드를 수정해야 할 수 있다.

   진입 시 fault가 발생한 주소는 CR2(Control Register 2)에 있고,
   fault에 대한 정보는 exception.h의 PF_* 매크로에 설명된 형식으로
   F의 error_code 멤버에 들어 있다. 여기의 예제 코드는 그 정보를
   파싱하는 방법을 보여준다. 자세한 내용은 [IA32-v3a] section 5.15
   "Exception and Interrupt Reference"의
   "Interrupt 14--Page Fault Exception (#PF)" 설명에서 볼 수 있다. */
static void
page_fault (struct intr_frame *f) {
	bool not_present;  /* true: 없는 페이지, false: 읽기 전용 페이지에 쓰기. */
	bool write;        /* true: 쓰기 접근, false: 읽기 접근. */
	bool user;         /* true: 유저 접근, false: 커널 접근. */
	void *fault_addr;  /* fault가 발생한 주소. */

	/* fault를 일으킨 접근 대상인 가상 주소를 얻는다.
	   이 주소는 코드나 데이터를 가리킬 수 있다.
	   반드시 fault를 일으킨 명령어의 주소는 아니다. 그 주소는 f->rip이다. */

	fault_addr = (void *) rcr2();

	/* interrupt를 다시 켠다. interrupt를 껐던 이유는 CR2가 바뀌기 전에
	   확실히 읽기 위해서뿐이다. */
	intr_enable ();


	/* 원인을 판별한다. */
	not_present = (f->error_code & PF_P) == 0;
	write = (f->error_code & PF_W) != 0;
	user = (f->error_code & PF_U) != 0;

#ifdef VM
	/* project 3 이후를 위한 코드. */
	if (vm_try_handle_fault (f, fault_addr, user, write, not_present))
		return;
#endif

	/* page fault 수를 센다. */
	page_fault_cnt++;

	/* 실제 fault라면 정보를 보여주고 종료한다. */
	printf ("Page fault at %p: %s error %s page in %s context.\n",
			fault_addr,
			not_present ? "not present" : "rights violation",
			write ? "writing" : "reading",
			user ? "user" : "kernel");
	kill (f);
}

