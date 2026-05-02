/* 128 kB 파일을 스택으로 읽어 들인 뒤 그 안의 바이트를 정렬한다. */

#include <debug.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/vm/qsort.h"

int
main (int argc UNUSED, char *argv[]) 
{
  test_name = "child-qsort";

  int handle;
  unsigned char buf[128 * 1024];
  size_t size;

  quiet = true;

  CHECK ((handle = open (argv[1])) > 1, "open \"%s\"", argv[1]);

  size = read (handle, buf, sizeof buf);
  qsort_bytes (buf, sizeof buf);
  seek (handle, 0);
  write (handle, buf, size);
  close (handle);
  
  return 72;
}
