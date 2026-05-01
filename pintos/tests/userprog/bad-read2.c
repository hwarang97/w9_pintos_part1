/* 이 프로그램은 커널 메모리를 읽으려고 시도한다. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  msg ("Congratulations - you have successfully read kernel memory: %d", 
        *(int *)0x8004000000);
  fail ("should have exited with -1");
}
