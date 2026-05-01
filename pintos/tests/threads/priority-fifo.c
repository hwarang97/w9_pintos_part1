/* 같은 우선순위의 여러 스레드를 만들고 FIFO 순서로 실행되는지 확인한다. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "devices/timer.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

struct simple_thread_data 
  {
    int id;                     /* 잠자는 스레드 ID. */
    int iterations;             /* 지금까지의 반복 횟수. */
    struct lock *lock;          /* 출력에 대한 락. */
    int **op;                   /* 출력 버퍼 위치. */
  };

#define THREAD_CNT 16
#define ITER_CNT 16

static thread_func simple_thread_func;

void
test_priority_fifo (void) 
{
  struct simple_thread_data data[THREAD_CNT];
  struct lock lock;
  int *output, *op;
  int i, cnt;

  /* 이 테스트는 MLFQS에서는 동작하지 않는다. */
  ASSERT (!thread_mlfqs);

  /* 현재 우선순위가 기본값인지 확인한다. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  msg ("%d threads will iterate %d times in the same order each time.",
       THREAD_CNT, ITER_CNT);
  msg ("If the order varies then there is a bug.");

  output = op = malloc (sizeof *output * THREAD_CNT * ITER_CNT * 2);
  ASSERT (output != NULL);
  lock_init (&lock);

  thread_set_priority (PRI_DEFAULT + 2);
  for (i = 0; i < THREAD_CNT; i++) 
    {
      char name[16];
      struct simple_thread_data *d = data + i;
      snprintf (name, sizeof name, "%d", i);
      d->id = i;
      d->iterations = 0;
      d->lock = &lock;
      d->op = &op;
      thread_create (name, PRI_DEFAULT + 1, simple_thread_func, d);
    }

  thread_set_priority (PRI_DEFAULT);
  /* 다른 모든 스레드는 이제 여기서 종료될 때까지 실행된다. */
  ASSERT (lock.holder == NULL);

  cnt = 0;
  for (; output < op; output++) 
    {
      struct simple_thread_data *d;

      ASSERT (*output >= 0 && *output < THREAD_CNT);
      d = data + *output;
      if (cnt % THREAD_CNT == 0)
        printf ("(priority-fifo) iteration:");
      printf (" %d", d->id);
      if (++cnt % THREAD_CNT == 0)
        printf ("\n");
      d->iterations++;
    }
}

static void 
simple_thread_func (void *data_) 
{
  struct simple_thread_data *data = data_;
  int i;
  
  for (i = 0; i < ITER_CNT; i++) 
    {
      lock_acquire (data->lock);
      *(*data->op)++ = data->id;
      lock_release (data->lock);
      thread_yield ();
    }
}
