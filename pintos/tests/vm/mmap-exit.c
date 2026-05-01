/* child-mm-wrt를 실행하고, 반영되어야 하는 쓰기가 실제로 반영되는지 검증한다. */

#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  pid_t child;

  /* 자식이 파일을 쓰게 한다. */
  quiet = true;
	child = fork("child-mm-wrt");
	if (child == 0) {
		CHECK ((child = exec ("child-mm-wrt")) != -1, "exec \"child-mm-wrt\"");
	} else {
		CHECK (wait (child) == 0, "wait for child (should return 0)");
		quiet = false;
		
		/* 파일 내용을 확인한다. */
		check_file ("sample.txt", sample, sizeof sample);
	} 
}
