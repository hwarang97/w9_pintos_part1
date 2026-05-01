/* 높은 우선순위 스레드가 실제로 선점하는지 확인한다. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func simple_thread_func;

void
test_priority_preempt (void) 
{
  /* 이 테스트는 MLFQS에서는 동작하지 않는다. */
  ASSERT (!thread_mlfqs);

  /* 현재 우선순위가 기본값인지 확인한다. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  thread_create ("high-priority", PRI_DEFAULT + 1, simple_thread_func, NULL);
  msg ("The high-priority thread should have already completed.");
}

static void 
simple_thread_func (void *aux UNUSED) 
{
  int i;
  
  for (i = 0; i < 5; i++) 
    {
      msg ("Thread %s iteration %d", thread_name (), i);
      thread_yield ();
    }
  msg ("Thread %s done!", thread_name ());
}
