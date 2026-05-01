#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"


/* 8254 timer chip의 hardware 세부 사항은 [8254]를 참고한다. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* OS가 boot된 뒤 지난 timer tick 수. */
static int64_t ticks;

/* timer tick 하나당 loop 수.
   timer_calibrate()가 초기화한다. */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static struct list sleep_list; // sleep thread를 모아두는 리스트 생성

/* 8254 Programmable Interval Timer(PIT)가 초당 PIT_FREQ번 interrupt를
   발생시키도록 설정하고, 해당 interrupt를 등록한다. */
void
timer_init (void) {         //OS가 실행될 때 모두 초기화하고 시작하는 함수 타이머 시스템 같은
	/* 8254 input frequency를 TIMER_FREQ로 나누고 가장 가까운 값으로 반올림한다. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb (0x43, 0x34);    /* CW: counter 0, LSB 다음 MSB, mode 2, binary. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);

	intr_register_ext (0x20, timer_interrupt, "8254 Timer");
	list_init(&sleep_list); //리스트 초기화
}

/* 짧은 delay 구현에 쓰이는 loops_per_tick을 보정한다. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* loops_per_tick을 timer tick 하나보다 작은 가장 큰 2의 거듭제곱으로 근사한다. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* loops_per_tick의 다음 8 bit를 다듬는다. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* OS가 boot된 뒤 지난 timer tick 수를 반환한다. */
//현재 OS시간
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level);
	barrier ();
	return t;
}

/* THEN 이후 지난 timer tick 수를 반환한다. THEN은 timer_ticks()가 반환했던
   값이어야 한다. */
int64_t
timer_elapsed (int64_t then) {
	return timer_ticks () - then;
}


/* 대략 TICKS timer tick 동안 실행을 중지한다. */
void
timer_sleep (int64_t ticks) {
	ASSERT (intr_get_level () == INTR_ON);
    if(ticks<=0){    //예외처리 바로 ticks 음수거나 0보다 작으면 바로 반환
		return;
	}
	int64_t now;  //현재 OS 기준 시간 받아주는 변수
	now=timer_ticks();  //현재 시간을 호출하는 함수 now에 저장
	thread_current()->wake_tick=now+ticks;  //현재 스레드 호출하는 함수 깨어날 시간 저장해주기
	enum intr_level old_level=intr_disable();  //interrupt 방해 방지 현재 interrupt를 저장하고 
	list_push_back(&sleep_list, &thread_current()->sleep_elem); //있는 라이브러르 함수 sleep_list 목록을 부르고 지금 스레드를 맨 뒤에 추가
	thread_block(); //스레드 블락
	intr_set_level(old_level);  //interrupt 방해해제 저장했던 interrupt를 다시 불러오기 old에는 on이 저장
}

/* 대략 MS millisecond 동안 실행을 중지한다. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* 대략 US microsecond 동안 실행을 중지한다. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* 대략 NS nanosecond 동안 실행을 중지한다. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* timer 통계를 출력한다. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++;
	struct list_elem *first=list_begin(&sleep_list); // sleep_list 첫 순환 변수 first로 생성
	struct list_elem *next;  //전체 순환을 위한 next변수 선언
	while(first!=list_end(&sleep_list)){   //전체 순회 시작은 first에 다음 노드는 next에 미리 넣어두는 방식
		next=list_next(first);
		struct thread *tr = list_entry(first, struct thread, sleep_elem); // 실제 원소를 thread로 복원하는 함수
		if(tr->wake_tick<=ticks){
			list_remove(&tr->sleep_elem);
			thread_unblock(tr);
		}
		first=next;    //마지막에 다시 next값을 first로 변경
	}
	thread_tick ();
}

/* LOOPS번 반복이 timer tick 하나보다 오래 걸리면 true, 아니면 false를 반환한다. */
static bool
too_many_loops (unsigned loops) {
	/* timer tick을 기다린다. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* LOOPS번 loop를 실행한다. */
	start = ticks;
	busy_wait (loops);

	/* tick count가 바뀌었다면 너무 오래 반복한 것이다. */
	barrier ();
	return start != ticks;
}

/* 짧은 delay를 구현하기 위해 단순 loop를 LOOPS번 반복한다.

   code alignment가 timing에 큰 영향을 줄 수 있으므로 NO_INLINE으로 표시한다.
   이 함수가 위치마다 다르게 inline되면 결과를 예측하기 어려워진다. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* 대략 NUM/DENOM초 동안 sleep한다. */
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* NUM/DENOM초를 timer tick으로 변환하고 내림한다.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* 최소 하나의 완전한 timer tick을 기다린다. CPU를 다른 process에
		   양보하기 위해 timer_sleep()을 사용한다. */
		timer_sleep (ticks);
	} else {
		/* 그렇지 않으면 더 정확한 sub-tick timing을 위해 busy-wait loop를 사용한다.
		   overflow 가능성을 피하려고 분자와 분모를 1000으로 줄인다. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}
