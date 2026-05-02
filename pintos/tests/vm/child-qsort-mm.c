/* 128 kB 파일을 mmap한 뒤 quick sort로 그 안의 바이트를 정렬한다. */

#include <debug.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/vm/qsort.h"

int
main (int argc UNUSED, char *argv[]) 
{
  test_name = "child-qsort-mm";

  int handle;
  unsigned char *p = (unsigned char *) 0x10000000;

  quiet = true;

  CHECK ((handle = open (argv[1])) > 1, "open \"%s\"", argv[1]);
  CHECK (mmap (p, 4096*33, 1, handle, 0) != MAP_FAILED, "mmap \"%s\"", argv[1]);
  qsort_bytes (p, 1024 * 128);
  
  return 80;
}
