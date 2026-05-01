/* 코드 세그먼트 위로 매핑하는 것이 금지되는지 검증한다. */

#include <stdint.h>
#include <round.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  uintptr_t test_main_page = ROUND_DOWN ((uintptr_t) test_main, 4096);
  int handle;
  
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK (mmap ((void *) test_main_page, 4096, 0, handle, 0) == MAP_FAILED,
         "try to mmap over code segment");
}

