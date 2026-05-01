/* 유효하지 않은 pid를 기다린다. 실패하거나 프로세스를 종료시킬 수 있다. */

#include <syscall.h>
#include "tests/main.h"

void
test_main (void) 
{
  wait ((pid_t) 0x0c020301);
}
