/* 128 kB 파일을 만들고 그 안의 데이터를 반복해서 섞는다. */

#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include "tests/arc4.h"
#include "tests/cksum.h"
#include "tests/lib.h"
#include "tests/main.h"

#define SIZE (128 * 1024)

static char *buf = (char *) 0x10000000;

void
test_main (void)
{
  size_t i;
  int handle;

  /* 파일을 만들고 mmap한다. */
  CHECK (create ("buffer", SIZE), "create \"buffer\"");
  CHECK ((handle = open ("buffer")) > 1, "open \"buffer\"");
  CHECK (mmap (buf, SIZE, 1, handle, 0) != MAP_FAILED, "mmap \"buffer\"");

  /* 초기화한다. */
  for (i = 0; i < SIZE; i++)
    buf[i] = i * 257;
  msg ("init: cksum=%lu", cksum (buf, SIZE));

  /* 반복해서 섞는다. */
  for (i = 0; i < 10; i++)
    {
      shuffle (buf, SIZE, 1);
      msg ("shuffle %zu: cksum=%lu", i, cksum (buf, SIZE));
    }
}
