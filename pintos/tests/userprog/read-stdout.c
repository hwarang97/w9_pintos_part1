/* fd 1(stdout)에서 읽기를 시도한다. */

#include <stdio.h>
#include <syscall.h>
#include "tests/main.h"

void
test_main (void) 
{
  char buf;
  read (STDOUT_FILENO, &buf, 1);
}
