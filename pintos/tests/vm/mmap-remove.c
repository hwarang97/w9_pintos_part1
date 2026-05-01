/* 메모리에 매핑된 파일을 삭제하고 닫는다. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char *actual = (char *) 0x10000000;
  int handle;
  void *map;
  size_t i;

  /* 파일을 매핑한다. */
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK ((map = mmap (actual, 4096, 0, handle, 0)) != MAP_FAILED, "mmap \"sample.txt\"");

  /* 파일을 닫고 삭제한다. */
  close (handle);
  CHECK (remove ("sample.txt"), "remove \"sample.txt\"");
  CHECK (open ("sample.txt") == -1, "try to open \"sample.txt\"");

  /* 이전 파일의 데이터를 덮어쓰기를 기대하며 새 파일을 만든다. */
  CHECK (create ("another", 4096 * 10), "create \"another\"");

  /* 매핑된 데이터가 올바른지 확인한다. */
  if (memcmp (actual, sample, strlen (sample)))
    fail ("read of mmap'd file reported bad data");

  /* 데이터 뒤에 0들이 이어지는지 검증한다. */
  for (i = strlen (sample); i < 4096; i++)
    if (actual[i] != 0)
      fail ("byte %zu of mmap'd region has value %02hhx (should be 0)",
            i, actual[i]);

  munmap (map);
}
