/* 존재하지 않는 프로세스 실행을 시도한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  msg ("exec(\"no-such-file\"): %d", exec ("no-such-file"));
}
