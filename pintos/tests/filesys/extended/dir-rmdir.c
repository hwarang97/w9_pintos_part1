/* 디렉터리를 만들고 제거한 뒤, 실제로 제거되었는지 확인한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  CHECK (mkdir ("a"), "mkdir \"a\"");
  CHECK (remove ("a"), "rmdir \"a\"");
  CHECK (!chdir ("a"), "chdir \"a\" (must return false)");
}
