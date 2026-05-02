/* halt 시스템 콜을 테스트한다. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  halt ();
  fail ("should have halted");
}
