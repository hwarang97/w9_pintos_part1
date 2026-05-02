/* 스택 세그먼트 위로 매핑하는 것이 금지되는지 검증한다. */

#include <stdint.h>
#include <round.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle;
  uintptr_t handle_page = ROUND_DOWN ((uintptr_t) &handle, 4096);
  
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK (mmap ((void *) handle_page, 4096, 0, handle, 0) == MAP_FAILED,
         "try to mmap over stack segment");
}

