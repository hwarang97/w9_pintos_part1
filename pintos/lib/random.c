#include "random.h"
#include <stdbool.h>
#include <stdint.h>
#include "debug.h"

/* RC4 기반 pseudo-random number generator(PRNG).

   RC4는 stream cipher다. 여기서는 암호학적 성질 때문이 아니라 구현하기 쉽고
   비암호학적 목적에는 출력이 충분히 random하기 때문에 사용한다.

   RC4 정보는 http://en.wikipedia.org/wiki/RC4_(cipher)를 참고한다.*/

/* RC4 상태. */
static uint8_t s[256];          /* S[]. */
static uint8_t s_i, s_j;        /* i, j. */

/* 이미 초기화되었는가? */
static bool inited;     

/* A와 B가 가리키는 byte를 맞바꾼다. */
static inline void
swap_byte (uint8_t *a, uint8_t *b) {
	uint8_t t = *a;
	*a = *b;
	*b = t;
}

/* 주어진 SEED로 PRNG를 초기화하거나 다시 초기화한다. */
void
random_init (unsigned seed) {
	uint8_t *seedp = (uint8_t *) &seed;
	int i;
	uint8_t j;

	for (i = 0; i < 256; i++) 
		s[i] = i;
	for (i = j = 0; i < 256; i++) {
		j += s[i] + seedp[i % sizeof seed];
		swap_byte (s + i, s + j);
	}

	s_i = s_j = 0;
	inited = true;
}

/* SIZE개의 random byte를 BUF에 쓴다. */
void
random_bytes (void *buf_, size_t size) {
	uint8_t *buf;

	if (!inited)
		random_init (0);

	for (buf = buf_; size-- > 0; buf++) {
		uint8_t s_k;

		s_i++;
		s_j += s[s_i];
		swap_byte (s + s_i, s + s_j);

		s_k = s[s_i] + s[s_j];
		*buf = s[s_k];
	}
}

/* pseudo-random unsigned long을 반환한다.
   0...n 범위의 random number를 얻으려면 random_ulong() % n을 사용한다.
   n은 포함되지 않는다. */
unsigned long
random_ulong (void) {
	unsigned long ul;
	random_bytes (&ul, sizeof ul);
	return ul;
}
