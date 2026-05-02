/* multi-child-fd 테스트가 실행하는 자식 프로세스. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include "tests/userprog/sample.inc"
#include "tests/lib.h"

int
main (int argc UNUSED, char *argv[]) 
{
  test_name = "child-close";

  msg ("begin");
  
  if (!isdigit (*argv[1]))
    fail ("bad command-line arguments");
  
  int handle = atoi (argv[1]);
  check_file_handle (handle, "sample.txt", sample, sizeof sample - 1);

  close (handle);
  msg ("end");

  return 0;
}
