/* 파일을 연 뒤 두 번 닫으려고 시도한다. 두 번째
   close must either fail silently or terminate with exit code
   -1. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle;
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  msg ("close \"sample.txt\"");
  close (handle);
  msg ("close \"sample.txt\" again");
  close (handle);
}
