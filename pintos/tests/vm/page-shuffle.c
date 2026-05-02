/* 128 kB 데이터 버퍼를 10번 섞고 각 단계의 체크섬을 출력한다. */

#include <stdbool.h>
#include "tests/arc4.h"
#include "tests/cksum.h"
#include "tests/lib.h"
#include "tests/main.h"

#define SIZE (128 * 1024)

static char buf[SIZE];

void
test_main (void)
{
  size_t i;

  /* 초기화한다. */
  for (i = 0; i < sizeof buf; i++)
    buf[i] = i * 257;
  msg ("init: cksum=%lu", cksum (buf, sizeof buf));
    
  /* 반복해서 섞는다. */
  for (i = 0; i < 10; i++)
    {
      shuffle (buf, sizeof buf, 1);
      msg ("shuffle %zu: cksum=%lu", i, cksum (buf, sizeof buf));
    }
}
