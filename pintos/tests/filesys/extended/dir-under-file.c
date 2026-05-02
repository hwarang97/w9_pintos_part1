/* 이미 존재하는 파일과 같은 이름으로 디렉터리 생성을 시도한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  CHECK (create ("abc", 0), "create \"abc\"");
  CHECK (!mkdir ("abc"), "mkdir \"abc\" (must return false)");
}
