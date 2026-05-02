/* page-parallel의 자식 프로세스. */

#include <string.h>
#include "tests/arc4.h"
#include "tests/lib.h"
#include "tests/main.h"

#define SIZE (1024 * 1024)
static char buf[SIZE];

int
main (int argc, char *argv[])
{
  test_name = "child-linear";

  const char *key = argv[argc - 1];
  struct arc4 arc4;
  size_t i;

  /* 0들을 암호화한다. */
  arc4_init (&arc4, key, strlen (key));
  arc4_crypt (&arc4, buf, SIZE);

  /* 다시 0들로 복호화한다. */
  arc4_init (&arc4, key, strlen (key));
  arc4_crypt (&arc4, buf, SIZE);

  /* 모두 0인지 확인한다. */
  for (i = 0; i < SIZE; i++)
    if (buf[i] != '\0')
      fail ("byte %zu != 0", i);

  return 0x42;
}
