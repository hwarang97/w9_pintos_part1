/* 실행 중인 프로세스의 실행 파일을 수정할 수 없는지 확인한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle;
  char buffer[16];
  
  CHECK ((handle = open ("rox-simple")) > 1, "open \"rox-simple\"");
  CHECK (read (handle, buffer, sizeof buffer) == (int) sizeof buffer,
         "read \"rox-simple\"");
  CHECK (write (handle, buffer, sizeof buffer) == 0,
         "try to write \"rox-simple\"");
}
