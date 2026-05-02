/* 하위 프로세스가 끝나기를 기다린다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int pid;
  if ((pid = fork ("child-simple"))){
    msg ("wait(exec()) = %d", wait (pid));
  } else {
    exec ("child-simple");
  }
}
