/* fd 0으로 mmap을 시도한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  CHECK (mmap ((void *) 0x10000000, 4096, 0, 0, 0) == MAP_FAILED,
         "try to mmap stdin");
}

