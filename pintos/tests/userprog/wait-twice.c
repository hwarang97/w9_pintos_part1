/* 하위 프로세스가 끝나기를 두 번 기다린다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  pid_t child;
  if ((child = fork ("child-simple"))){
    msg ("wait(exec()) = %d", wait (child));
    msg ("wait(exec()) = %d", wait (child));
  } else {
    exec ("child-simple");
  }
}
