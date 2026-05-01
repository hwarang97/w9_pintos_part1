/* timer_sleep(-100)을 테스트한다. 충돌하지 않기만 하면 된다. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

void
test_alarm_negative (void) 
{
  timer_sleep (-100);
  pass ();
}
