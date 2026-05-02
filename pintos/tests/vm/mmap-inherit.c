/* 파일을 메모리에 매핑하고 child-inherit를 실행해 매핑 상속 동작을 검증한다. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char *actual = (char *) 0x54321000;
  int handle;
  pid_t child;

  /* 파일을 열고 매핑한 뒤 데이터를 검증한다. */
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK (mmap (actual, 4096, 0, handle, 0) != MAP_FAILED, "mmap \"sample.txt\"");
  if (memcmp (actual, sample, strlen (sample)))
    fail ("read of mmap'd file reported bad data");

	/* 자식을 생성하고 기다린다. */
	child = fork("child-inherit");
	if (child == 0) {
		CHECK (exec ("child-inherit") != -1, "exec \"child-inherit\"");
	}	else {
		quiet = true;
		CHECK (wait (child) == -1, "wait for child (should return -1)");
		quiet = false;
	}

  /* 데이터를 다시 검증한다. */
  CHECK (!memcmp (actual, sample, strlen (sample)),
         "checking that mmap'd file still has same data");
}
