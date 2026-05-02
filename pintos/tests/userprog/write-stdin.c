/* fd 0(stdin)에 쓰기를 시도한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  char buf = 123;
  write (0, &buf, 1);
}
