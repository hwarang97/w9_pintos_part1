/* 길이 0 파일의 매핑을 시도한다. 성공 여부와 관계없이 이후 동작을 확인한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char *data = (char *) 0x7f000000;
  int handle;

  CHECK (create ("empty", 0), "create empty file \"empty\"");
  CHECK ((handle = open ("empty")) > 1, "open \"empty\"");

  /* mmap() 호출은 성공할 수도 실패할 수도 있다. 여기서는 신경 쓰지 않는다. */
  msg ("mmap \"empty\"");
  mmap (data, 0, 0, handle, 0);

  /* 호출 성공 여부와 관계없이 *data 접근은 예외를 일으켜야 한다. */
  fail ("unmapped memory is readable (%d)", *data);
}

