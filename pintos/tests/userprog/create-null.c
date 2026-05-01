/* 널 포인터를 이름으로 사용해 파일 생성을 시도한다. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  msg ("create(NULL): %d", create (NULL, 0));
}
