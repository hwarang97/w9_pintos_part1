/* 이름이 두 페이지 경계에 걸쳐 있는 스레드를 fork한다. */

#include <syscall.h>
#include "tests/userprog/boundary.h"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  pid_t pid = fork (copy_string_across_boundary ("child-simple"));
  if (pid == 0){
    msg ("child run");
    exit(54);
  } else {
    int exit_val = wait(pid);
    CHECK (pid > 0, "fork");
    CHECK (exit_val == 54, "wait");
  }
}
