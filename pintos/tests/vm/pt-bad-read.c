/* 파일에서 읽은 데이터를 잘못된 주소에 쓰려고 한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  int handle;

  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  read (handle, (char *) &handle - 4096, 1);
  fail ("survived reading data into bad address");
}
