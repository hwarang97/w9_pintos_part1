/* 모든 종류의 메모리가 제대로 swap out되고 swap in되는지 확인한다. 이 테스트에서 Pintos 메모리 크기는 128MB다. */

#include <string.h>
#include <syscall.h>
#include <stdio.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/vm/large.inc"


#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define ONE_MB (1 << 20) // 1MB
#define CHUNK_SIZE (20*ONE_MB)
#define PAGE_COUNT (CHUNK_SIZE / PAGE_SIZE)

static char big_chunks[CHUNK_SIZE];

void
test_main (void)
{
    size_t i, handle;
    char *actual = (char *) 0x10000000;
    void *map;

    for (i = 0 ; i < PAGE_COUNT ; i++) {
        if ((i & 0x1ff) == 0)
            msg ("write sparsely over page %zu", i);
        big_chunks[i*PAGE_SIZE] = (char) i;
    }

    CHECK ((handle = open ("large.txt")) > 1, "open \"large.txt\"");
    CHECK ((map = mmap (actual, sizeof(large), 0, handle, 0)) != MAP_FAILED, "mmap \"large.txt\"");

    /* 파일 매핑 페이지를 읽어 들인다 */
    if (memcmp (actual, large, strlen (large)))
        fail ("read of mmap'd file reported bad data");


    /* 익명 페이지를 읽어 들인다 */
    for (i = 0; i < PAGE_COUNT; i++) {
        if (big_chunks[i*PAGE_SIZE] != (char) i)
            fail ("data is inconsistent");
        if ((i & 0x1ff) == 0)
            msg ("check consistency in page %zu", i);
    }

    /* 파일 매핑 페이지를 다시 확인한다 */
    if (memcmp (actual, large, strlen (large)))
        fail ("read of mmap'd file reported bad data");

    /* 매핑을 해제하고 열린 파일을 닫는다 */
    munmap (map);
    close (handle);
}
