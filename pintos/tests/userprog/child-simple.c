/* exec-multiple, exec-one, wait-simple 등이 실행하는 자식 프로세스. */

#include <stdio.h>
#include "tests/lib.h"

int
main (void) 
{
  test_name = "child-simple";

  msg ("run");
  return 81;
}
