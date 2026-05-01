/* 유효하지 않은 fd에 쓰려고 시도한다. */

#include <limits.h>
#include <syscall.h>
#include "tests/main.h"

void
test_main (void) 
{
  char buf = 123;
  write (0x01012342, &buf, 1);
  write (7, &buf, 1);
  write (2546, &buf, 1);
  write (-5, &buf, 1);
  write (-8192, &buf, 1);
  write (INT_MIN + 1, &buf, 1);
  write (INT_MAX - 1, &buf, 1);
}
