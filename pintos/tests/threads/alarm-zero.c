/* timer_sleep(0)이 즉시 반환해야 함을 테스트한다. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

void
test_alarm_zero (void) 
{
  timer_sleep (0);
  pass ();
}
