/* 0바이트 쓰기를 시도한다. 실제 쓰기 없이 0을 반환해야 한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle, byte_cnt;
  char buf;

  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");

  buf = 123;
  byte_cnt = write (handle, &buf, 0);
  if (byte_cnt != 0)
    fail("write() returned %d instead of 0", byte_cnt);
}
