#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* counting semaphore. */
struct semaphore {
	unsigned value;             /* 현재 값. */
	struct list waiters;        /* 대기 중인 스레드 목록. */
};

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* lock. */
struct lock {
	struct thread *holder;      /* lock을 가진 스레드, 디버깅용. */
	struct semaphore semaphore; /* 접근을 제어하는 binary semaphore. */
};

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* condition variable. */
struct condition {
	struct list waiters;        /* 대기 중인 스레드 목록. */
};

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* optimization barrier.
 *
 * 컴파일러는 optimization barrier를 가로질러 연산 순서를 바꾸지 않는다.
 * 자세한 내용은 reference guide의 "Optimization Barriers"를 참고한다. */
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
