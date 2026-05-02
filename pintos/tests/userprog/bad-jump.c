/* 이 프로그램은 매핑되지 않은 주소 0의 코드를 실행하려고 시도한다. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  msg ("Congratulations - you have successfully called NULL: %d", 
        ((int (*)(void))NULL)());
  fail ("should have exited with -1");
}
