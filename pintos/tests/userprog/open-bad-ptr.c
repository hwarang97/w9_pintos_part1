/* open 시스템 콜에 유효하지 않은 포인터를 넘긴다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  msg ("open(0x20101234): %d", open ((char *) 0x20101234));
  fail ("should have called exit(-1)");
}
