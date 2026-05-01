/* 시스템 콜 중에도 스택이 올바르게 확장되는지 확인한다. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  int handle;
  int slen = strlen (sample);
  char buf2[65536];

  /* write()를 통해 파일에 쓴다. */
  CHECK (create ("sample.txt", slen), "create \"sample.txt\"");
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK (write (handle, sample, slen) == slen, "write \"sample.txt\"");
  close (handle);

  /* read()로 다시 읽는다. */
  CHECK ((handle = open ("sample.txt")) > 1, "2nd open \"sample.txt\"");
  CHECK (read (handle, buf2 + 32768, slen) == slen, "read \"sample.txt\"");

  CHECK (!memcmp (sample, buf2 + 32768, slen), "compare written data against read data");
  close (handle);
}
