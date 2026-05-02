/* 메인 스레드가 락을 획득한 뒤, 락 획득에서 블록되는 더 높은 우선순위 스레드 두 개를 만든다. 이 스레드들은 메인 스레드에 우선순위를 기부해야 한다. 메인 스레드가 락을 해제하면 다른 스레드들은 우선순위 순서대로 락을 획득해야 한다.\n\n   이 테스트는 Stanford CS 140 겨울 1999 수업에 Matt Franklin, Greg Hutchins, Yu Ping Hu가 제출한 테스트를 바탕으로 하며, arens가 수정했다. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func acquire1_thread_func;
static thread_func acquire2_thread_func;

void
test_priority_donate_one (void) 
{
  struct lock lock;

  /* 이 테스트는 MLFQS에서는 동작하지 않는다. */
  ASSERT (!thread_mlfqs);

  /* 현재 우선순위가 기본값인지 확인한다. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&lock);
  lock_acquire (&lock);
  thread_create ("acquire1", PRI_DEFAULT + 1, acquire1_thread_func, &lock);
  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());
  thread_create ("acquire2", PRI_DEFAULT + 2, acquire2_thread_func, &lock);
  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());
  lock_release (&lock);
  msg ("acquire2, acquire1 must already have finished, in that order.");
  msg ("This should be the last line before finishing this test.");
}

static void
acquire1_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("acquire1: got the lock");
  lock_release (lock);
  msg ("acquire1: done");
}

static void
acquire2_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("acquire2: got the lock");
  lock_release (lock);
  msg ("acquire2: done");
}
