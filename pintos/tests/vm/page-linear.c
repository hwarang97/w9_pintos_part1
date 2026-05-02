/* 2MB 메모리를 암호화한 뒤 복호화하고 내용이 올바른지 검증한다. */

#include <string.h>
#include "tests/arc4.h"
#include "tests/lib.h"
#include "tests/main.h"

#define SIZE (5 * 1024 * 1024)

static char buf[SIZE];

void
test_main (void)
{
  struct arc4 arc4;
  size_t i;

  /* 0x5a로 초기화한다. */
  msg ("initialize");
  memset (buf, 0x5a, sizeof buf);

  /* 모두 0x5a인지 확인한다. */
  msg ("read pass");
  for (i = 0; i < SIZE; i++)
    if (buf[i] != 0x5a)
      fail ("byte %zu != 0x5a", i);

  /* 0들을 암호화한다. */
  msg ("read/modify/write pass one");
  arc4_init (&arc4, "foobar", 6);
  arc4_crypt (&arc4, buf, SIZE);

  /* 다시 0들로 복호화한다. */
  msg ("read/modify/write pass two");
  arc4_init (&arc4, "foobar", 6);
  arc4_crypt (&arc4, buf, SIZE);

  /* 모두 0x5a인지 확인한다. */
  msg ("read pass");
  for (i = 0; i < SIZE; i++)
    if (buf[i] != 0x5a)
      fail ("byte %zu != 0x5a", i);
}
