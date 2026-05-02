/* 이 프로그램은 매핑되지 않은 주소의 메모리를 읽으려고 시도한다. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  msg ("Congratulations - you have successfully dereferenced NULL: %d", 
        *(int *)NULL);
  fail ("should have exited with -1");
}
