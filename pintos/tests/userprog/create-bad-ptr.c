/* create 시스템 콜에 잘못된 포인터를 넘긴다. 이 경우 프로세스는 종료 코드 -1로 종료되어야 한다. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  msg ("create(0x20101234): %d", create ((char *) 0x20101234, 0));
}
