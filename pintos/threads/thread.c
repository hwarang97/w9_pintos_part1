#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* struct thread의 `magic' 멤버에 넣는 임의 값.
   stack overflow를 감지하는 데 사용한다. 자세한 내용은 thread.h 위쪽의
   큰 주석을 참고한다. */
#define THREAD_MAGIC 0xcd6abf4b

/* basic thread용 임의 값.
   이 값은 수정하지 말 것. */
#define THREAD_BASIC 0xd42df210

/* THREAD_READY 상태의 process list. 즉 실행할 준비는 되었지만 실제로 실행 중은
   아닌 process들이다. */
static struct list ready_list;

/* idle thread. */
static struct thread *idle_thread;

/* initial thread, 즉 init.c:main()을 실행하는 thread. */
static struct thread *initial_thread;

/* allocate_tid()가 사용하는 lock. */
static struct lock tid_lock;

/* thread 파괴 요청. */
static struct list destruction_req;

/* 통계. */
static long long idle_ticks;   /* idle 상태로 보낸 timer tick 수. */
static long long kernel_ticks; /* kernel thread에서 보낸 timer tick 수. */
static long long user_ticks;   /* user program에서 보낸 timer tick 수. */

/* scheduling. */
#define TIME_SLICE 4		  /* 각 thread에 줄 timer tick 수. */
static unsigned thread_ticks; /* 마지막 yield 이후 timer tick 수. */

/* false(기본값)이면 round-robin scheduler를 사용한다.
   true이면 multi-level feedback queue scheduler를 사용한다.
   kernel command-line option "-o mlfqs"가 제어한다. */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule(void);
static tid_t allocate_tid(void);
bool thread_priority_vs(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED); // 비교 함수 선언 thread_vs const타입 비교하는 원소의 값을 수정하지 못하게 제어 즉 읽기 전용
static int load_avg;																			 //
static struct list all_list;																	 // recent_cpu가 전체 thread에 적용되어야하기에

/* T가 유효한 thread를 가리키는 것처럼 보이면 true를 반환한다. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* 실행 중인 thread를 반환한다.
 * CPU의 stack pointer `rsp'를 읽고 page 시작 지점으로 내림한다.
 * `struct thread'는 항상 page 시작 부분에 있고 stack pointer는 중간 어딘가에
 * 있으므로, 이를 통해 현재 thread를 찾는다. */
#define running_thread() ((struct thread *)(pg_round_down(rrsp())))

// thread_start용 global descriptor table.
// gdt는 thread_init 이후 설정되므로 먼저 임시 gdt를 설정해야 한다.
static uint64_t gdt[3] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff};

/* 현재 실행 중인 코드를 thread로 바꾸어 threading system을 초기화한다.
   일반적으로는 이것이 동작할 수 없지만, 이 경우에는 loader.S가 stack 바닥을
   page boundary에 조심스럽게 두었기 때문에 가능하다.

   run queue와 tid lock도 초기화한다.

   이 함수를 호출한 뒤 thread_create()로 thread를 만들기 전에 반드시 page
   allocator를 초기화해야 한다.

   이 함수가 끝나기 전까지는 thread_current()를 호출하는 것이 안전하지 않다. */
void thread_init(void)
{
	ASSERT(intr_get_level() == INTR_OFF);

	/* kernel용 임시 gdt를 다시 load한다.
	 * 이 gdt는 user context를 포함하지 않는다.
	 * kernel은 gdt_init()에서 user context를 포함해 gdt를 다시 만든다. */
	struct desc_ptr gdt_ds = {
		.size = sizeof(gdt) - 1,
		.address = (uint64_t)gdt};
	lgdt(&gdt_ds);

	/* global thread context를 초기화한다. */
	lock_init(&tid_lock);
	list_init(&ready_list);
	list_init(&destruction_req);
	list_init(&all_list);

	/* 실행 중인 thread용 thread 구조체를 설정한다. */
	initial_thread = running_thread();
	init_thread(initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid();
}

/* interrupt를 활성화해 preemptive thread scheduling을 시작한다.
   idle thread도 만든다. */
void thread_start(void)
{
	/* idle thread를 만든다. */
	struct semaphore idle_started;
	sema_init(&idle_started, 0);
	thread_create("idle", PRI_MIN, idle, &idle_started);

	/* preemptive thread scheduling을 시작한다. */
	intr_enable();

	/* idle thread가 idle_thread를 초기화할 때까지 기다린다. */
	sema_down(&idle_started);
}

/* timer interrupt handler가 매 timer tick마다 호출한다.
   따라서 이 함수는 external interrupt context에서 실행된다. */
void thread_tick(void)
{
	struct thread *t = thread_current();

	/* 통계를 갱신한다. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* preemption을 강제한다. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return();
}

/* thread 통계를 출력한다. */
void thread_print_stats(void)
{
	printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
		   idle_ticks, kernel_ticks, user_ticks);
}

/* 주어진 초기 PRIORITY를 가진 NAME이라는 새 kernel thread를 만든다.
   이 thread는 AUX를 인자로 전달해 FUNCTION을 실행하고 ready queue에 추가된다.
   새 thread의 식별자를 반환하거나 생성이 실패하면 TID_ERROR를 반환한다.

   thread_start()가 호출된 상태라면 새 thread는 thread_create()가 반환되기 전에
   schedule될 수 있다. 심지어 thread_create()가 반환되기 전에 종료될 수도 있다.
   반대로 새 thread가 schedule되기 전까지 기존 thread가 얼마든지 실행될 수도
   있다. 순서를 보장해야 한다면 semaphore나 다른 형태의 동기화를 사용한다.

   제공된 코드는 새 thread의 `priority' 멤버를 PRIORITY로 설정하지만, 실제
   priority scheduling은 구현되어 있지 않다. priority scheduling은 Problem 1-3의
   목표다. */
tid_t thread_create(const char *name, int priority,
					thread_func *function, void *aux)
{
	struct thread *t;
	tid_t tid;

	ASSERT(function != NULL);

	/* thread를 할당한다. */
	t = palloc_get_page(PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* thread를 초기화한다. */
	init_thread(t, name, priority);
	tid = t->tid = allocate_tid();

	/* schedule되면 kernel_thread를 호출한다.
	 * 참고) rdi는 첫 번째 인자이고 rsi는 두 번째 인자다. */
	t->tf.rip = (uintptr_t)kernel_thread;
	t->tf.R.rdi = (uint64_t)function;
	t->tf.R.rsi = (uint64_t)aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* run queue에 추가한다. */
	thread_unblock(t);
	if (t->priority > thread_current()->priority)
	{					// 새로 만들어진 스레드의 우선순위가 현재 스레드의 우선순위보다 높다면 thread_yield()를 호출한다.
		thread_yield(); // 현재 실행 중인 스레드가 호출하는 주체
	}

	return tid;
}

/* 현재 thread를 sleep 상태로 둔다. thread_unblock()으로 깨어날 때까지 다시
   schedule되지 않는다.

   이 함수는 interrupt가 꺼진 상태에서 호출되어야 한다. 보통은 synch.h의
   동기화 primitive 중 하나를 사용하는 편이 더 낫다. */
void thread_block(void)
{
	ASSERT(!intr_context());
	ASSERT(intr_get_level() == INTR_OFF);
	thread_current()->status = THREAD_BLOCKED;
	schedule();
}

/* blocked thread T를 ready-to-run 상태로 전환한다.
   T가 blocked 상태가 아니면 오류다. 실행 중인 thread를 ready 상태로 만들려면
   thread_yield()를 사용한다.

   이 함수는 실행 중인 thread를 preempt하지 않는다. 이는 중요할 수 있다.
   호출자가 직접 interrupt를 비활성화했다면, thread unblock과 다른 data 갱신을
   atomic하게 할 수 있다고 기대할 수 있기 때문이다. */
void thread_unblock(struct thread *t)
{
	enum intr_level old_level; // interrupt 저장 변수

	ASSERT(is_thread(t));

	old_level = intr_disable();											  // 기존 interrupt 상태를 저장하고 interrupt를 비활성화
	ASSERT(t->status == THREAD_BLOCKED);								  // 새로 들어온 스레드가 block상태가 맞는지 확인
	list_insert_ordered(&ready_list, &t->elem, thread_priority_vs, NULL); // ready_list에 우선순위 기준으로 정렬 삽입, aux는 사용하지 않으므로 NULL
	t->status = THREAD_READY;											  // 상태변경 ready상태로
	intr_set_level(old_level);											  // 저장해둔 interrupt 상태로 복구
}

/* 실행 중인 thread의 이름을 반환한다. */
const char *
thread_name(void)
{
	return thread_current()->name;
}

/* 실행 중인 thread를 반환한다.
   running_thread()에 몇 가지 sanity check를 더한 것이다.
   자세한 내용은 thread.h 위쪽의 큰 주석을 참고한다. */
struct thread *
thread_current(void)
{
	struct thread *t = running_thread();

	/* T가 정말 thread인지 확인한다.
	   이 assertion 중 하나라도 발생하면 thread가 stack overflow를 일으켰을 수 있다.
	   각 thread의 stack은 4 kB보다 작으므로, 큰 automatic array 몇 개나 적당한
	   recursion만으로도 stack overflow가 발생할 수 있다. */
	ASSERT(is_thread(t));
	ASSERT(t->status == THREAD_RUNNING);

	return t;
}

/* 실행 중인 thread의 tid를 반환한다. */
tid_t thread_tid(void)
{
	return thread_current()->tid;
}

/* 현재 thread를 deschedule하고 파괴한다. 호출자에게 절대 반환하지 않는다. */
void thread_exit(void)
{
	ASSERT(!intr_context());

#ifdef USERPROG
	process_exit();
#endif

	/* status를 dying으로 설정하고 다른 process를 schedule한다.
	   schedule_tail() 호출 중에 파괴될 것이다. */
	intr_disable();
	do_schedule(THREAD_DYING);
	NOT_REACHED();
}

/* CPU를 yield한다. 현재 thread는 sleep 상태가 되지 않으며 scheduler 판단에 따라
   즉시 다시 schedule될 수 있다. */
void thread_yield(void)
{
	struct thread *curr = thread_current();
	enum intr_level old_level;

	ASSERT(!intr_context());

	old_level = intr_disable();
	if (curr != idle_thread)
		list_insert_ordered(&ready_list, &curr->elem, thread_priority_vs, NULL); // 정렬 삽입으로 구조 개선
	do_schedule(THREAD_READY);
	intr_set_level(old_level);
}

/* 현재 thread의 priority를 NEW_PRIORITY로 설정한다. */
void thread_set_priority(int new_priority)
{
	thread_current()->priority = new_priority;
	if (!list_empty(&ready_list))
	{																// 먼저 list_ready가 비어 있는지 체크 //front사용시 오류
		struct list_elem *front = list_front(&ready_list);			// 맨 앞에 있는 원소 보기
		struct thread *ft = list_entry(front, struct thread, elem); // 원소를 통해서 thread주소로 접근
		if (thread_current()->priority < ft->priority)
		{					// 현재 스레드랑 우선순위 비교
			thread_yield(); // 현재 스레드보다 ready_list 맨 앞의 원소의 우선순위가 더 높다면 therad_yield()호출
		}
	}
}

/* 현재 thread의 priority를 반환한다. */
int thread_get_priority(void)
{
	return thread_current()->priority;
}

/* 현재 thread의 nice 값을 NICE로 설정한다. */
void thread_set_nice(int nice UNUSED)
{
	/* TODO: 여기에 구현을 작성한다. */
}

/* 현재 thread의 nice 값을 반환한다. */
int thread_get_nice(void)
{
	/* TODO: 여기에 구현을 작성한다. */
	return 0;
}

/* system load average의 100배 값을 반환한다. */
int thread_get_load_avg(void)
{
	/* TODO: 여기에 구현을 작성한다. */
	return 0;
}

/* 현재 thread의 recent_cpu 값의 100배를 반환한다. */
int thread_get_recent_cpu(void)
{
	/* TODO: 여기에 구현을 작성한다. */
	return 0;
}

/* idle thread. 실행할 준비가 된 다른 thread가 없을 때 실행된다.

   idle thread는 처음에 thread_start()에 의해 ready list에 들어간다. 처음 한 번
   schedule되면 idle_thread를 초기화하고, thread_start()가 계속 진행할 수 있도록
   전달받은 semaphore를 "up"한 뒤 즉시 block한다. 그 뒤 idle thread는 ready list에
   나타나지 않는다. ready list가 비었을 때 next_thread_to_run()이 특수한 경우로
   반환한다. */
static void
idle(void *idle_started_ UNUSED)
{
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current();
	sema_up(idle_started);

	for (;;)
	{
		/* 다른 thread가 실행되게 한다. */
		intr_disable();
		thread_block();

		/* interrupt를 다시 활성화하고 다음 interrupt를 기다린다.

		   `sti' instruction은 다음 instruction이 완료될 때까지 interrupt를
		   비활성화하므로, 이 두 instruction은 atomic하게 실행된다. 이 atomic성은
		   중요하다. 그렇지 않으면 interrupt를 다시 활성화하고 다음 interrupt를
		   기다리는 사이에 interrupt가 처리되어 clock tick 하나만큼의 시간이
		   낭비될 수 있다.

		   [IA32-v2a] "HLT", [IA32-v2b] "STI", [IA32-v3a] 7.11.1
		   "HLT Instruction"을 참고한다. */
		asm volatile("sti; hlt" : : : "memory");
	}
}

/* kernel thread의 기반으로 사용되는 함수. */
static void
kernel_thread(thread_func *function, void *aux)
{
	ASSERT(function != NULL);

	intr_enable(); /* scheduler는 interrupt가 꺼진 상태로 실행된다. */
	function(aux); /* thread 함수를 실행한다. */
	thread_exit(); /* function()이 반환하면 thread를 죽인다. */
}

/* T를 NAME이라는 blocked thread로 기본 초기화한다. */
static void
init_thread(struct thread *t, const char *name, int priority)
{
	ASSERT(t != NULL);
	ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT(name != NULL);

	memset(t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy(t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t)t + PGSIZE - sizeof(void *);
	t->priority = priority;
	t->magic = THREAD_MAGIC;
	if (t == initial_thread)
	{ // 부모가 없는 첫 main thread라면
		t->nice = 0;
		t->recent_cpu = 0;
	}
	else
	{												  // 일반 새 thread라면
		t->nice = thread_current()->nice;			  // 부모 스레드의 nice값을 그대로 가져온다
		t->recent_cpu = thread_current()->recent_cpu; // 부모 스레드의 recent_cpu값을 그대로 가져온다.
		t->pml4 = NULL;                               // 유저 메모리 공간을 가리키는 테이블
		t->exit_status = -1;                          // 비정상 종료 상태로 -1로 설정
	}
}

/* 다음에 schedule할 thread를 선택해 반환한다. run queue가 비어 있지 않다면
   run queue에서 thread를 반환해야 한다. 실행 중인 thread가 계속 실행될 수 있다면
   run queue 안에 있을 것이다. run queue가 비어 있으면 idle_thread를 반환한다. */
static struct thread *
next_thread_to_run(void)
{
	if (list_empty(&ready_list))
		return idle_thread;
	else
		return list_entry(list_pop_front(&ready_list), struct thread, elem);
}

/* iretq를 사용해 thread를 시작한다. */
void do_iret(struct intr_frame *tf)
{
	__asm __volatile(
		"movq %0, %%rsp\n"
		"movq 0(%%rsp),%%r15\n"
		"movq 8(%%rsp),%%r14\n"
		"movq 16(%%rsp),%%r13\n"
		"movq 24(%%rsp),%%r12\n"
		"movq 32(%%rsp),%%r11\n"
		"movq 40(%%rsp),%%r10\n"
		"movq 48(%%rsp),%%r9\n"
		"movq 56(%%rsp),%%r8\n"
		"movq 64(%%rsp),%%rsi\n"
		"movq 72(%%rsp),%%rdi\n"
		"movq 80(%%rsp),%%rbp\n"
		"movq 88(%%rsp),%%rdx\n"
		"movq 96(%%rsp),%%rcx\n"
		"movq 104(%%rsp),%%rbx\n"
		"movq 112(%%rsp),%%rax\n"
		"addq $120,%%rsp\n"
		"movw 8(%%rsp),%%ds\n"
		"movw (%%rsp),%%es\n"
		"addq $32, %%rsp\n"
		"iretq"
		: : "g"((uint64_t)tf) : "memory");
}

/* 새 thread의 page table을 활성화해 thread를 전환하고, 이전 thread가 dying 상태라면
   그것을 파괴한다.

   이 함수가 호출되는 시점에는 thread PREV에서 막 전환된 상태이며,
   새 thread는 이미 실행 중이고 interrupt는 아직 비활성화되어 있다.

   thread switch가 완료될 때까지 printf() 호출은 안전하지 않다.
   실제로는 printf()를 함수 끝에 추가해야 한다는 뜻이다. */
static void
thread_launch(struct thread *th)
{
	uint64_t tf_cur = (uint64_t)&running_thread()->tf;
	uint64_t tf = (uint64_t)&th->tf;
	ASSERT(intr_get_level() == INTR_OFF);

	/* 주요 switching logic.
	 * 먼저 전체 execution context를 intr_frame으로 복원한 뒤 do_iret를 호출해
	 * 다음 thread로 전환한다.
	 * 여기서부터 switch가 끝날 때까지 어떤 stack도 사용하면 안 된다는 점에 주의한다. */
	__asm __volatile(
		/* 사용할 register를 저장한다. */
		"push %%rax\n"
		"push %%rbx\n"
		"push %%rcx\n"
		/* input을 한 번 가져온다. */
		"movq %0, %%rax\n"
		"movq %1, %%rcx\n"
		"movq %%r15, 0(%%rax)\n"
		"movq %%r14, 8(%%rax)\n"
		"movq %%r13, 16(%%rax)\n"
		"movq %%r12, 24(%%rax)\n"
		"movq %%r11, 32(%%rax)\n"
		"movq %%r10, 40(%%rax)\n"
		"movq %%r9, 48(%%rax)\n"
		"movq %%r8, 56(%%rax)\n"
		"movq %%rsi, 64(%%rax)\n"
		"movq %%rdi, 72(%%rax)\n"
		"movq %%rbp, 80(%%rax)\n"
		"movq %%rdx, 88(%%rax)\n"
		"pop %%rbx\n" // 저장된 rcx
		"movq %%rbx, 96(%%rax)\n"
		"pop %%rbx\n" // 저장된 rbx
		"movq %%rbx, 104(%%rax)\n"
		"pop %%rbx\n" // 저장된 rax
		"movq %%rbx, 112(%%rax)\n"
		"addq $120, %%rax\n"
		"movw %%es, (%%rax)\n"
		"movw %%ds, 8(%%rax)\n"
		"addq $32, %%rax\n"
		"call __next\n" // 현재 rip를 읽는다.
		"__next:\n"
		"pop %%rbx\n"
		"addq $(out_iret -  __next), %%rbx\n"
		"movq %%rbx, 0(%%rax)\n" // rip
		"movw %%cs, 8(%%rax)\n"	 // cs
		"pushfq\n"
		"popq %%rbx\n"
		"mov %%rbx, 16(%%rax)\n" // eflags
		"mov %%rsp, 24(%%rax)\n" // rsp
		"movw %%ss, 32(%%rax)\n"
		"mov %%rcx, %%rdi\n"
		"call do_iret\n"
		"out_iret:\n"
		: : "g"(tf_cur), "g"(tf) : "memory");
}

/* 새 process를 schedule한다. 진입 시 interrupt는 꺼져 있어야 한다.
 * 이 함수는 현재 thread의 status를 status로 바꾼 뒤 실행할 다른 thread를 찾아
 * 전환한다. schedule() 안에서 printf()를 호출하는 것은 안전하지 않다. */
static void
do_schedule(int status)
{
	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(thread_current()->status == THREAD_RUNNING);
	while (!list_empty(&destruction_req))
	{
		struct thread *victim =
			list_entry(list_pop_front(&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current()->status = status;
	schedule();
}

static void
schedule(void)
{
	struct thread *curr = running_thread();
	struct thread *next = next_thread_to_run();

	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(curr->status != THREAD_RUNNING);
	ASSERT(is_thread(next));
	/* 실행 중이라고 표시한다. */
	next->status = THREAD_RUNNING;

	/* 새 time slice를 시작한다. */
	thread_ticks = 0;

#ifdef USERPROG
	/* 새 address space를 활성화한다. */
	process_activate(next);
#endif

	if (curr != next)
	{
		/* 전환 전 thread가 dying 상태라면 그 struct thread를 파괴한다.
		   thread_exit()가 자기 발밑을 직접 없애지 않도록 이 작업은 늦게 일어나야 한다.
		   현재 page가 stack으로 사용 중이므로 여기서는 page free request만 queue에
		   넣는다. 실제 파괴 logic은 schedule()의 시작 부분에서 호출된다. */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread)
		{
			ASSERT(curr != next);
			list_push_back(&destruction_req, &curr->elem);
		}

		/* thread를 전환하기 전에 먼저 현재 실행 정보를 저장한다. */
		thread_launch(next);
	}
}

/* 새 thread에 사용할 tid를 반환한다. */
static tid_t
allocate_tid(void)
{
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire(&tid_lock);
	tid = next_tid++;
	lock_release(&tid_lock);

	return tid;
}
bool thread_priority_vs(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{															// 비교함수
	struct thread *ta = list_entry(a, struct thread, elem); // a elem을 포함하는 thread를 찾음
	struct thread *tb = list_entry(b, struct thread, elem); // b elem을 포함하는 thread를 찾음
	if (ta->priority > tb->priority)
	{				 // ta와 tb의 우선순위를 비교
		return true; // 반환 타입 bool true일 경우 b앞에 삽입
	}
	else
	{
		return false; // false일 경우는 b를 지나가 다음 대상이랑 비교 반복
	}
}
