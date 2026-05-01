/* 128 kB 파일을 정적 데이터로 읽어 들인 뒤 그 안의 바이트를 정렬한다. */

#include <debug.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

unsigned char buf[128 * 1024];
size_t histogram[256];

int
main (int argc UNUSED, char *argv[]) 
{
  test_name = "child-sort";

  int handle;
  unsigned char *p;
  size_t size;
  size_t i;

  quiet = true;

  CHECK ((handle = open (argv[1])) > 1, "open \"%s\"", argv[1]);

  size = read (handle, buf, sizeof buf);
  for (i = 0; i < size; i++)
    histogram[buf[i]]++;
  p = buf;
  for (i = 0; i < sizeof histogram / sizeof *histogram; i++) 
    {
      size_t j = histogram[i];
      while (j-- > 0)
        *p++ = i;
    }
  seek (handle, 0);
  write (handle, buf, size);
  close (handle);
  
  return 123;
}
