/* 데이터 세그먼트 위로 매핑하는 것이 금지되는지 검증한다. */

#include <stdint.h>
#include <round.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

static char x;

void
test_main (void) 
{
  uintptr_t x_page = ROUND_DOWN ((uintptr_t) &x, 4096);
  int handle;
  
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK (mmap ((void *) x_page, 4096, 0, handle, 0) == MAP_FAILED,
         "try to mmap over data segment");
}

