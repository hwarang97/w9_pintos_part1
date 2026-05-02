/* 이 파일은 교육용 운영체제 Nachos의 소스 코드에서 파생되었다.
   Nachos 저작권 고지는 아래에 그대로 재현되어 있다. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* semaphore SEMA를 VALUE로 초기화한다. semaphore는 음수가 아닌 정수와
   이를 조작하는 두 atomic operator로 이루어진다.

   - down 또는 "P": 값이 양수가 될 때까지 기다린 뒤 감소시킨다.

   - up 또는 "V": 값을 증가시킨다. 기다리는 thread가 있으면 하나를 깨운다. */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* semaphore에 대한 down 또는 "P" 연산. SEMA의 값이 양수가 될 때까지 기다린 뒤
   atomic하게 감소시킨다.

   이 함수는 sleep할 수 있으므로 interrupt handler 안에서 호출하면 안 된다.
   interrupt가 비활성화된 상태에서 호출될 수는 있지만, sleep하면 다음으로
   schedule된 thread가 아마 interrupt를 다시 켤 것이다. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		list_insert_ordered(&sema->waiters, &thread_current()->elem,thread_priority_vs, NULL); //정렬삽입 코드로 변경 
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* semaphore에 대한 down 또는 "P" 연산이지만, semaphore가 아직 0이 아닐 때만
   수행한다. semaphore가 감소되면 true, 아니면 false를 반환한다.

   이 함수는 interrupt handler에서 호출될 수 있다. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* semaphore에 대한 up 또는 "V" 연산. SEMA의 값을 증가시키고, SEMA를 기다리는
   thread가 있으면 그중 하나를 깨운다.

   이 함수는 interrupt handler에서 호출될 수 있다. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if(!list_empty(&sema->waiters)){ //리스트가 비어 있지 않다면
		struct list_elem *front=list_pop_front(&sema->waiters);  //리스트에서 가장 앞에 원소를 꺼내와서 front에 저장한다
		struct thread *ft=list_entry(front, struct thread, elem); //front원소에서 thread를 반환받아 ft에 저장한다
		thread_unblock(ft);  // ft를 ready_list로 보낸다
		if(ft->priority>thread_current()->priority){  //ft스레드의 우선순위가 현재 실행중인 스레드보다 우선순위가 높다면
			if(intr_context()){   //지금 인터럽트 핸들러 안에 있다면
				sema->value++;
	            intr_set_level (old_level);
				intr_yield_on_return(); //끝나고 yield를 호출하도록 예약해라
			}
			else{
				sema->value++;
	            intr_set_level (old_level);  
				thread_yield(); //아니라면 바로 yield호출
			}
			return; 
	}
}
	sema->value++;
	intr_set_level (old_level);
}
static void sema_test_helper (void *sema_);

/* 두 thread 사이에서 제어가 "ping-pong"되게 만드는 semaphore self-test.
   무슨 일이 일어나는지 보려면 printf() 호출을 넣으면 된다. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* sema_self_test()가 사용하는 thread 함수. */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* LOCK을 초기화한다. lock은 어떤 시점에도 최대 하나의 thread만 가질 수 있다.
   우리의 lock은 "recursive"가 아니다. 즉 현재 lock을 가진 thread가 그 lock을
   다시 acquire하려는 것은 오류다.

   lock은 초기값이 1인 semaphore를 특수화한 것이다. lock과 그런 semaphore의
   차이는 두 가지다. 첫째, semaphore는 1보다 큰 값을 가질 수 있지만 lock은
   한 번에 하나의 thread만 소유할 수 있다. 둘째, semaphore에는 소유자가 없어서
   한 thread가 semaphore를 "down"하고 다른 thread가 "up"할 수 있지만,
   lock에서는 같은 thread가 acquire와 release를 모두 해야 한다. 이런 제약이
   부담스럽다면 lock 대신 semaphore를 써야 한다는 좋은 신호다. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* LOCK을 acquire한다. 필요하면 사용 가능해질 때까지 sleep한다.
   현재 thread가 이미 이 lock을 가지고 있으면 안 된다.

   이 함수는 sleep할 수 있으므로 interrupt handler 안에서 호출하면 안 된다.
   interrupt가 비활성화된 상태에서 호출될 수는 있지만, sleep이 필요하면
   interrupt가 다시 켜질 것이다. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));
	if(lock->holder!=NULL){ //lock을 잡고 있는 스레드가 존재한다면
		if(thread_current()->priority>lock->holder->priority){ // 현재 진행 중인 스레드보다 그 락을 잡고있는 스레드의 우선순위가 높다면
			lock->holder->priority=thread_current()->priority; // 락을 잡고있는 스레드의 우선순위에 현재 진행중인 스레드의 우선순위를 기부한다.
		}
	}

	sema_down (&lock->semaphore);
	lock->holder = thread_current ();
}

/* LOCK acquire를 시도하고 성공하면 true, 실패하면 false를 반환한다.
   현재 thread가 이미 이 lock을 가지고 있으면 안 된다.

   이 함수는 sleep하지 않으므로 interrupt handler 안에서 호출될 수 있다. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* LOCK을 release한다. LOCK은 현재 thread가 소유하고 있어야 한다.

   interrupt handler는 lock을 acquire할 수 없으므로 interrupt handler 안에서
   lock release를 시도하는 것은 의미가 없다. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* 현재 thread가 LOCK을 가지고 있으면 true, 아니면 false를 반환한다.
   다른 thread가 lock을 가지고 있는지 검사하는 것은 race가 될 수 있음에 주의한다. */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* list 안의 semaphore 하나. */
struct semaphore_elem {
	struct list_elem elem;              /* list 원소. */
	struct semaphore semaphore;         /* 이 semaphore. */
};

/* condition variable COND를 초기화한다. condition variable은 한 코드 조각이
   condition을 signal하고 협력하는 코드가 그 signal을 받아 동작할 수 있게 한다. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* LOCK을 atomic하게 release하고 다른 코드 조각이 COND를 signal할 때까지 기다린다.
   COND가 signal된 뒤에는 반환하기 전에 LOCK을 다시 acquire한다.
   이 함수를 호출하기 전에는 LOCK을 가지고 있어야 한다.

   이 함수가 구현하는 monitor는 "Hoare" style이 아니라 "Mesa" style이다.
   즉 signal을 보내고 받는 일이 atomic operation이 아니다. 따라서 보통 호출자는
   wait가 끝난 뒤 condition을 다시 확인하고, 필요하면 다시 기다려야 한다.

   주어진 condition variable은 하나의 lock에만 연결되지만, 하나의 lock은 여러
   condition variable과 연결될 수 있다. 즉 lock에서 condition variable로
   one-to-many mapping이 있다.

   이 함수는 sleep할 수 있으므로 interrupt handler 안에서 호출하면 안 된다.
   interrupt가 비활성화된 상태에서 호출될 수는 있지만, sleep이 필요하면
   interrupt가 다시 켜질 것이다. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	list_push_back (&cond->waiters, &waiter.elem);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* LOCK으로 보호되는 COND에서 기다리는 thread가 있으면, 이 함수는 그중 하나를
   signal해 wait에서 깨어나게 한다. 이 함수를 호출하기 전에는 LOCK을 가지고
   있어야 한다.

   interrupt handler는 lock을 acquire할 수 없으므로 interrupt handler 안에서
   condition variable에 signal을 보내려는 것은 의미가 없다. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
}

/* LOCK으로 보호되는 COND에서 기다리는 모든 thread를 깨운다.
   이 함수를 호출하기 전에는 LOCK을 가지고 있어야 한다.

   interrupt handler는 lock을 acquire할 수 없으므로 interrupt handler 안에서
   condition variable에 signal을 보내려는 것은 의미가 없다. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
