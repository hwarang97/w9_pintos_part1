/* 유효하지 않은 fd 닫기를 시도한다. 조용히 실패하거나 프로세스를 종료해야 한다. */

#include <syscall.h>
#include "tests/main.h"

void
test_main (void) 
{
  close (0x20101234);
}
