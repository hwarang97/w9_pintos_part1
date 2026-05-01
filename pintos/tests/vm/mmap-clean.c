/* mmap된 영역이 munmap 시점에만 다시 기록되는지 검증한다. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  static const char overwrite[] = "Now is the time for all good...";
  static char buffer[sizeof sample - 1];
  char *actual = (char *) 0x54321000;
  int handle;
  void *map;

  /* 파일을 열고 매핑한 뒤 데이터를 검증한다. */
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
 
	CHECK ((map = mmap (actual, 4096, 0, handle, 0)) != MAP_FAILED, "mmap \"sample.txt\"");
  if (memcmp (actual, sample, strlen (sample)))
    fail ("read of mmap'd file reported bad data");

  /* 파일을 수정한다. */
  CHECK (write (handle, overwrite, strlen (overwrite))
         == (int) strlen (overwrite),
         "write \"sample.txt\"");

  /* 매핑을 닫는다. 여기서는 munmap하지 않았으므로 데이터가 다시 기록되면 안 된다. */
  msg ("munmap \"sample.txt\"");
  munmap (map);

  /* 파일을 다시 읽는다. */
  msg ("seek \"sample.txt\"");
  seek (handle, 0);
  CHECK (read (handle, buffer, sizeof buffer) == sizeof buffer,
         "read \"sample.txt\"");

  /* 파일 덮어쓰기가 동작했는지 검증한다. */
  if (memcmp (buffer, overwrite, strlen (overwrite))
      || memcmp (buffer + strlen (overwrite), sample + strlen (overwrite),
                 strlen (sample) - strlen (overwrite))) 
    {
      if (!memcmp (buffer, sample, strlen (sample)))
        fail ("munmap wrote back clean page");
      else
        fail ("read surprising data from file"); 
    }
  else
    msg ("file change was retained after munmap");
}
