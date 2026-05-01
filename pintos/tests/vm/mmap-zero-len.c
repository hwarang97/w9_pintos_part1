/* 길이 0으로 mmap을 시도한다. Linux 2.6.12 이전에는 길이 0 mmap이 성공했지만, Pintos에서는 길이가 0이면 MAP_FAILED를 반환해야 한다. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

#define ACTUAL ((void *) 0x10000000)

void
test_main (void)
{
  int handle;
  void *map;

  /* mmap을 통해 파일에 쓴다. */
  CHECK (create ("sample.txt", strlen (sample)), "create \"sample.txt\"");
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK ((map = mmap (ACTUAL, 0, 0, handle, 0)) == MAP_FAILED, 
			"try to mmap zero length");
 
}
