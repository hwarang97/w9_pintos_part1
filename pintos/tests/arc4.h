#ifndef TESTS_ARC4_H
#define TESTS_ARC4_H

#include <stddef.h>
#include <stdint.h>

/* RC4로 알려진 알고리즘의 암호화 상태. */
struct arc4
  {
    uint8_t s[256];
    uint8_t i, j;
  };

void arc4_init (struct arc4 *, const void *, size_t);
void arc4_crypt (struct arc4 *, void *, size_t);

#endif /* tests/arc4.h */
