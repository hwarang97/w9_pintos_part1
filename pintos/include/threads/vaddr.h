#ifndef THREADS_VADDR_H
#define THREADS_VADDR_H

#include <debug.h>
#include <stdint.h>
#include <stdbool.h>

#include "threads/loader.h"

/* 가상 주소를 다루기 위한 함수와 매크로.
 *
 * x86 하드웨어 페이지 테이블 전용 함수와 매크로는 pte.h를 참고한다. */

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* page offset, bit 0:12. */
#define PGSHIFT 0                          /* 첫 offset bit의 index. */
#define PGBITS  12                         /* offset bit 수. */
#define PGSIZE  (1 << PGBITS)              /* page 하나의 byte 수. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* page offset bit, 0:12. */

/* page 안에서의 offset. */
#define pg_ofs(va) ((uint64_t) (va) & PGMASK)

#define pg_no(va) ((uint64_t) (va) >> PGBITS)

/* 가장 가까운 page 경계까지 올림한다. */
#define pg_round_up(va) ((void *) (((uint64_t) (va) + PGSIZE - 1) & ~PGMASK))

/* 가장 가까운 page 경계까지 내림한다. */
#define pg_round_down(va) (void *) ((uint64_t) (va) & ~PGMASK)

/* 커널 가상 주소 시작. */
#define KERN_BASE LOADER_KERN_BASE

/* 유저 스택 시작. */
#define USER_STACK 0x47480000

/* VADDR이 유저 가상 주소이면 true를 반환한다. */
#define is_user_vaddr(vaddr) (!is_kernel_vaddr((vaddr)))

/* VADDR이 커널 가상 주소이면 true를 반환한다. */
#define is_kernel_vaddr(vaddr) ((uint64_t)(vaddr) >= KERN_BASE)

// FIXME: 검사 추가 필요
/* 물리 주소 PADDR이 매핑된 커널 가상 주소를 반환한다. */
#define ptov(paddr) ((void *) (((uint64_t) paddr) + KERN_BASE))

/* 커널 가상 주소 VADDR이 매핑된 물리 주소를 반환한다. */
#define vtop(vaddr) \
({ \
	ASSERT(is_kernel_vaddr(vaddr)); \
	((uint64_t) (vaddr) - (uint64_t) KERN_BASE);\
})

#endif /* threads/vaddr.h */
