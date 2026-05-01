#ifndef THREADS_LOADER_H
#define THREADS_LOADER_H

/* PC BIOS가 고정해 둔 상수들. */
#define LOADER_BASE 0x7c00      /* loader 시작의 물리 주소. */
#define LOADER_END  0x7e00      /* loader 끝의 물리 주소. */

/* kernel base의 물리 주소. */
#define LOADER_KERN_BASE 0x8004000000

/* 모든 물리 메모리가 매핑되는 커널 가상 주소. */
#define LOADER_PHYS_BASE 0x200000

/* multiboot 정보. */
#define MULTIBOOT_INFO       0x7000
#define MULTIBOOT_FLAG       MULTIBOOT_INFO
#define MULTIBOOT_MMAP_LEN   MULTIBOOT_INFO + 44
#define MULTIBOOT_MMAP_ADDR  MULTIBOOT_INFO + 48

#define E820_MAP MULTIBOOT_INFO + 52
#define E820_MAP4 MULTIBOOT_INFO + 56

/* 중요한 loader 물리 주소들. */
#define LOADER_SIG (LOADER_END - LOADER_SIG_LEN)   /* 0xaa55 BIOS signature. */
#define LOADER_ARGS (LOADER_SIG - LOADER_ARGS_LEN)     /* command-line 인자. */
#define LOADER_ARG_CNT (LOADER_ARGS - LOADER_ARG_CNT_LEN) /* 인자 개수. */

/* loader 자료구조 크기. */
#define LOADER_SIG_LEN 2
#define LOADER_ARGS_LEN 128
#define LOADER_ARG_CNT_LEN 4

/* loader가 정의한 GDT selector.
   더 많은 selector는 userprog/gdt.h에 정의되어 있다. */
#define SEL_NULL        0x00    /* null selector. */
#define SEL_KCSEG       0x08    /* 커널 코드 selector. */
#define SEL_KDSEG       0x10    /* 커널 데이터 selector. */
#define SEL_UDSEG       0x1B    /* 유저 데이터 selector. */
#define SEL_UCSEG       0x23    /* 유저 코드 selector. */
#define SEL_TSS         0x28    /* task-state segment. */
#define SEL_CNT         8       /* segment 개수. */

#endif /* threads/loader.h */
