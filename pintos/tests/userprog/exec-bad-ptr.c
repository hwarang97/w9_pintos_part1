/* exec 시스템 콜에 유효하지 않은 포인터를 넘긴다. */

#include <syscall.h>
#include "tests/main.h"

void
test_main (void) 
{
  exec ((char *) 0x20101234);
}
