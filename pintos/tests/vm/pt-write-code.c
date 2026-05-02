/* 코드 세그먼트에 쓰기를 시도한다. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  *(int *) test_main = 0;
  fail ("writing the code segment succeeded");
}
