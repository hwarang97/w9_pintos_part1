/* 널 포인터를 이름으로 사용해 파일 열기를 시도한다. */

#include <stddef.h>
#include <syscall.h>
#include "tests/main.h"

void
test_main (void) 
{
  open (NULL);
}
