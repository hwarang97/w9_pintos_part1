/* wait-killed 테스트가 실행하는 자식 프로세스. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  exec ("pintos");
  fail ("should have exited with -1");
}
