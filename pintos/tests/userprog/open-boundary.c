/* 이름이 두 페이지 경계에 걸쳐 있는 파일을 만든다. */

#include <syscall.h>
#include "tests/userprog/boundary.h"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  CHECK (open (copy_string_across_boundary ("sample.txt")) > 1,
         "open \"sample.txt\"");
}
