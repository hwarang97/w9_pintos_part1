/* 빈 문자열을 이름으로 사용해 디렉터리 생성을 시도한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  CHECK (!mkdir (""), "mkdir \"\" (must return false)");
}
