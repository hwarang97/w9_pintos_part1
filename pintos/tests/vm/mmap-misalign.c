/* 정렬되지 않은 메모리 매핑이 금지되는지 검증한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle;
  
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK (mmap ((void *) 0x10001234, 4096, 0, handle, 0) == MAP_FAILED,
         "try to mmap at misaligned address");
}

