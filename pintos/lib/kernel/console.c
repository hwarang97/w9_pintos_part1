#include <console.h>
#include <stdarg.h>
#include <stdio.h>
#include "devices/serial.h"
#include "devices/vga.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/synch.h"

static void vprintf_helper (char, void *);
static void putchar_have_lock (uint8_t c);

/* 콘솔 락.
   vga와 serial 계층은 각각 자체 락을 사용하므로 언제든 호출해도 안전하다.
   하지만 동시에 실행되는 printf() 호출의 출력이 섞여 혼란스러워지는 것을
   막는 데 이 락이 유용하다. */
static struct lock console_lock;

/* 보통 상황에서는 true다. 위에서 설명했듯이 스레드 사이의 출력이 섞이는
   것을 피하기 위해 콘솔 락을 사용하려는 것이다.

   락이 동작할 수 있거나 콘솔 락이 초기화되기 전의 초기 부팅 단계,
   또는 커널 패닉 이후에는 false다.  전자의 경우 락을 잡으려 하면
   assertion 실패가 발생하고, 다시 패닉이 발생해 후자의 경우가 된다.
   후자의 경우 패닉의 원인이 잘못된 lock_acquire() 구현이라면 대개
   재귀에 빠질 뿐이다. */
static bool use_console_lock;

/* pintos에 디버그 출력을 충분히 추가하면, 단일 스레드에서 console_lock을
   재귀적으로 잡으려 할 수 있다.  실제 예로 palloc_free()에 printf()
   호출을 추가했을 때 다음과 같은 backtrace가 나왔다:

   lock_console()
   vprintf()
   printf()             - palloc()이 락을 다시 잡으려 한다
   palloc_free()        
   schedule_tail()      - 스레드를 전환하는 중 다른 스레드가 종료된다
   schedule()
   thread_yield()
   intr_handler()       - 타이머 인터럽트
   intr_set_level()
   serial_putc()
   putchar_have_lock()
   putbuf()
   sys_write()          - 한 프로세스가 콘솔에 쓴다
   syscall_handler()
   intr_handler()

   이런 문제는 디버그하기 매우 어렵기 때문에, 깊이 카운터로 재귀 락을
   흉내 내어 문제를 피한다. */
static int console_lock_depth;

/* 콘솔에 쓴 문자 수. */
static int64_t write_cnt;

/* 콘솔 락 사용을 활성화한다. */
void
console_init (void) {
	lock_init (&console_lock);
	use_console_lock = true;
}

/* 커널 패닉이 진행 중임을 콘솔에 알린다.
   이제부터 콘솔 락을 잡으려 하지 말라는 의미다. */
void
console_panic (void) {
	use_console_lock = false;
}

/* 콘솔 통계를 출력한다. */
void
console_print_stats (void) {
	printf ("Console: %lld characters output\n", write_cnt);
}

/* 콘솔 락을 획득한다. */
	static void
acquire_console (void) {
	if (!intr_context () && use_console_lock) {
		if (lock_held_by_current_thread (&console_lock)) 
			console_lock_depth++; 
		else
			lock_acquire (&console_lock); 
	}
}

/* 콘솔 락을 해제한다. */
static void
release_console (void) {
	if (!intr_context () && use_console_lock) {
		if (console_lock_depth > 0)
			console_lock_depth--;
		else
			lock_release (&console_lock); 
	}
}

/* 현재 스레드가 콘솔 락을 가지고 있으면 true, 아니면 false를 반환한다. */
static bool
console_locked_by_current_thread (void) {
	return (intr_context ()
			|| !use_console_lock
			|| lock_held_by_current_thread (&console_lock));
}

/* 표준 vprintf() 함수.
   printf()와 비슷하지만 va_list를 사용한다.
   출력을 vga 디스플레이와 직렬 포트 양쪽에 쓴다. */
int
vprintf (const char *format, va_list args) {
	int char_cnt = 0;

	acquire_console ();
	__vprintf (format, args, vprintf_helper, &char_cnt);
	release_console ();

	return char_cnt;
}

/* 문자열 S를 콘솔에 쓰고 그 뒤에 새 줄 문자를 쓴다. */
int
puts (const char *s) {
	acquire_console ();
	while (*s != '\0')
		putchar_have_lock (*s++);
	putchar_have_lock ('\n');
	release_console ();

	return 0;
}

/* BUFFER의 N개 문자를 콘솔에 쓴다. */
void
putbuf (const char *buffer, size_t n) {
	acquire_console ();
	while (n-- > 0)
		putchar_have_lock (*buffer++);
	release_console ();
}

/* C를 vga 디스플레이와 직렬 포트에 쓴다. */
int
putchar (int c) {
	acquire_console ();
	putchar_have_lock (c);
	release_console ();

	return c;
}

/* vprintf()를 돕는 함수. */
static void
vprintf_helper (char c, void *char_cnt_) {
	int *char_cnt = char_cnt_;
	(*char_cnt)++;
	putchar_have_lock (c);
}

/* C를 vga 디스플레이와 직렬 포트에 쓴다.
   필요하다면 호출자가 이미 콘솔 락을 획득한 상태다. */
static void
putchar_have_lock (uint8_t c) {
	ASSERT (console_locked_by_current_thread ());
	write_cnt++;
	serial_putc (c);
	vga_putc (c);
}
