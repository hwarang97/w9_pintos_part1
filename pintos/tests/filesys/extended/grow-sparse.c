/* 파일 끝을 넘어 seek한 뒤 쓰면 파일이 올바르게 커지는지 테스트한다. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

static char buf[76543];

void
test_main (void) 
{
  const char *file_name = "testfile";
  char zero = 0;
  int fd;
  
  CHECK (create (file_name, 0), "create \"%s\"", file_name);
  CHECK ((fd = open (file_name)) > 1, "open \"%s\"", file_name);
  msg ("seek \"%s\"", file_name);
  seek (fd, sizeof buf - 1);
  CHECK (write (fd, &zero, 1) > 0, "write \"%s\"", file_name);
  msg ("close \"%s\"", file_name);
  close (fd);
  check_file (file_name, buf, sizeof buf);
}
