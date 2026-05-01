/* 0바이트 읽기를 시도한다. 실제 읽기 없이 0을 반환해야 한다. */

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
  byte_cnt = read (handle, &buf, 0);
  if (byte_cnt != 0)
    fail ("read() returned %d instead of 0", byte_cnt);
  else if (buf != 123)
    fail ("0-byte read() modified buffer");
}
