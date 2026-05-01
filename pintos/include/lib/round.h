#ifndef __LIB_ROUND_H
#define __LIB_ROUND_H

/* X를 STEP의 가장 가까운 배수로 올림한 값을 낸다.
 * X >= 0, STEP >= 1인 경우에만 사용한다. */
#define ROUND_UP(X, STEP) (((X) + (STEP) - 1) / (STEP) * (STEP))

/* X를 STEP으로 나눈 값을 올림해서 낸다.
 * X >= 0, STEP >= 1인 경우에만 사용한다. */
#define DIV_ROUND_UP(X, STEP) (((X) + (STEP) - 1) / (STEP))

/* X를 STEP의 가장 가까운 배수로 내림한 값을 낸다.
 * X >= 0, STEP >= 1인 경우에만 사용한다. */
#define ROUND_DOWN(X, STEP) ((X) / (STEP) * (STEP))

/* DIV_ROUND_DOWN은 없다. 단순히 X / STEP과 같기 때문이다. */

#endif /* lib/round.h */
