#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

/* 예외 원인을 설명하는 page fault error code bit. */
#define PF_P 0x1    /* 0: 없는 페이지. 1: 접근 권한 위반. */
#define PF_W 0x2    /* 0: 읽기, 1: 쓰기. */
#define PF_U 0x4    /* 0: 커널, 1: 유저 프로세스. */

void exception_init (void);
void exception_print_stats (void);

#endif /* userprog/exception.h */
