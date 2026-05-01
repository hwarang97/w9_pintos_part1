/* 메인 스레드가 락 A와 B를 획득한 뒤 세 스레드를 만들어 다중 기부를 테스트한다. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func a_thread_func;
static thread_func b_thread_func;
static thread_func c_thread_func;

void
test_priority_donate_multiple2 (void) 
{
  struct lock a, b;

  /* 이 테스트는 MLFQS에서는 동작하지 않는다. */
  ASSERT (!thread_mlfqs);

  /* 현재 우선순위가 기본값인지 확인한다. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&a);
  lock_init (&b);

  lock_acquire (&a);
  lock_acquire (&b);

  thread_create ("a", PRI_DEFAULT + 3, a_thread_func, &a);
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 3, thread_get_priority ());

  thread_create ("c", PRI_DEFAULT + 1, c_thread_func, NULL);

  thread_create ("b", PRI_DEFAULT + 5, b_thread_func, &b);
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 5, thread_get_priority ());

  lock_release (&a);
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 5, thread_get_priority ());

  lock_release (&b);
  msg ("Threads b, a, c should have just finished, in that order.");
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT, thread_get_priority ());
}

static void
a_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("Thread a acquired lock a.");
  lock_release (lock);
  msg ("Thread a finished.");
}

static void
b_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("Thread b acquired lock b.");
  lock_release (lock);
  msg ("Thread b finished.");
}

static void
c_thread_func (void *a_ UNUSED) 
{
  msg ("Thread c finished.");
}
