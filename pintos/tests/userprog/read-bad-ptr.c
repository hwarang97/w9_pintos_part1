/* read 시스템 콜에 유효하지 않은 포인터를 넘긴다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle;
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");

  read (handle, (char *) 0xc0100000, 123);
  fail ("should not have survived read()");
}
