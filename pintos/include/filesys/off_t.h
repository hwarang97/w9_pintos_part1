#ifndef FILESYS_OFF_T_H
#define FILESYS_OFF_T_H

#include <stdint.h>

/* 파일 안의 오프셋.
 * 여러 헤더가 이 정의만 필요로 하고 다른 정의는 필요로 하지 않기 때문에
 * 별도 헤더로 분리되어 있다. */
typedef int32_t off_t;

/* printf()용 format specifier 예:
 * printf ("offset=%"PROTd"\n", offset); */
#define PROTd PRId32

#endif /* filesys/off_t.h */
