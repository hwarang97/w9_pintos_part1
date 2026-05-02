/* 현재 디렉터리의 부모 제거를 시도한다. 이 작업은 실패해야 한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  CHECK (mkdir ("a"), "mkdir \"a\"");
  CHECK (chdir ("a"), "chdir \"a\"");
  CHECK (mkdir ("b"), "mkdir \"b\"");
  CHECK (chdir ("b"), "chdir \"b\"");
  CHECK (!remove ("/a"), "remove \"/a\" (must fail)");
}
