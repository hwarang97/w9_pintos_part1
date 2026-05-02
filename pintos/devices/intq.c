#include "devices/intq.h"
#include <debug.h>
#include "threads/thread.h"

static int next (int pos);
static void wait (struct intq *q, struct thread **waiter);
static void signal (struct intq *q, struct thread **waiter);

/* interrupt queue Q를 초기화한다. */
void
intq_init (struct intq *q) {
	lock_init (&q->lock);
	q->not_full = q->not_empty = NULL;
	q->head = q->tail = 0;
}

/* Q가 비어 있으면 true, 아니면 false를 반환한다. */
bool
intq_empty (const struct intq *q) {
	ASSERT (intr_get_level () == INTR_OFF);
	return q->head == q->tail;
}

/* Q가 가득 차 있으면 true, 아니면 false를 반환한다. */
bool
intq_full (const struct intq *q) {
	ASSERT (intr_get_level () == INTR_OFF);
	return next (q->head) == q->tail;
}

/* Q에서 byte를 제거해 반환한다.
   interrupt handler에서 호출된다면 Q는 비어 있으면 안 된다.
   그 외의 경우 Q가 비어 있으면 byte가 추가될 때까지 먼저 sleep한다. */
uint8_t
intq_getc (struct intq *q) {
	uint8_t byte;

	ASSERT (intr_get_level () == INTR_OFF);
	while (intq_empty (q)) {
		ASSERT (!intr_context ());
		lock_acquire (&q->lock);
		wait (q, &q->not_empty);
		lock_release (&q->lock);
	}

	byte = q->buf[q->tail];
	q->tail = next (q->tail);
	signal (q, &q->not_full);
	return byte;
}

/* BYTE를 Q의 끝에 추가한다.
   interrupt handler에서 호출된다면 Q는 가득 차 있으면 안 된다.
   그 외의 경우 Q가 가득 차 있으면 byte가 제거될 때까지 먼저 sleep한다. */
void
intq_putc (struct intq *q, uint8_t byte) {
	ASSERT (intr_get_level () == INTR_OFF);
	while (intq_full (q)) {
		ASSERT (!intr_context ());
		lock_acquire (&q->lock);
		wait (q, &q->not_full);
		lock_release (&q->lock);
	}

	q->buf[q->head] = byte;
	q->head = next (q->head);
	signal (q, &q->not_empty);
}

/* intq 안에서 POS 다음 위치를 반환한다. */
static int
next (int pos) {
	return (pos + 1) % INTQ_BUFSIZE;
}

/* WAITER는 Q의 not_empty 또는 not_full 멤버 주소여야 한다.
   주어진 조건이 true가 될 때까지 기다린다. */
static void
wait (struct intq *q UNUSED, struct thread **waiter) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT ((waiter == &q->not_empty && intq_empty (q))
			|| (waiter == &q->not_full && intq_full (q)));

	*waiter = thread_current ();
	thread_block ();
}

/* WAITER는 Q의 not_empty 또는 not_full 멤버 주소여야 하며,
   관련 조건은 true여야 한다. thread가 그 조건을 기다리고 있으면 깨우고
   waiting thread를 초기화한다. */
static void
signal (struct intq *q UNUSED, struct thread **waiter) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT ((waiter == &q->not_empty && !intq_empty (q))
			|| (waiter == &q->not_full && !intq_full (q)));

	if (*waiter != NULL) {
		thread_unblock (*waiter);
		*waiter = NULL;
	}
}
