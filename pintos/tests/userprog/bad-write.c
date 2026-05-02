/* 이 프로그램은 매핑되지 않은 주소의 메모리에 쓰려고 시도한다. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  *(int *)NULL = 42;
  fail ("should have exited with -1");
}
