/* 이 프로그램은 커널 메모리에 쓰려고 시도한다. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  *(int *)0x8004000000 = 42;
  fail ("should have exited with -1");
}
