/* 익명 페이지가 지연 로드되는지 확인한다. */

#include <string.h>
#include <syscall.h>
#include <stdio.h>
#include <stdint.h>
#include "tests/lib.h"
#include "tests/main.h"

#define PAGE_SIZE 4096
#define CHUNK_PAGE_COUNT 3
#define CHUNK_SIZE (CHUNK_PAGE_COUNT * PAGE_SIZE)

static char buf[CHUNK_SIZE];

void
test_main (void)
{
	size_t i, j;
	void *pa;

	msg ("initial pages status");
	for (i = 0 ; i < CHUNK_PAGE_COUNT ; i++) {
		// buf의 모든 페이지는 아직 로드되지 않아야 한다.
		pa = get_phys_addr(&buf[i*PAGE_SIZE]);
		CHECK (pa == 0, "check if page is not loaded");
	}

	msg ("load pages");
	for (i = 0 ; i < CHUNK_PAGE_COUNT ; i++) {
		msg ("load page [%zu]", i);
		// 여기에서 페이지가 로드된다.
		buf[i*PAGE_SIZE] = i;
		for (j = 0 ; j < CHUNK_PAGE_COUNT ; j++) {
			// 접근한 페이지는 로드되어 있어야 한다
			// 접근하지 않은 페이지는 로드되어 있지 않아야 한다.
			pa = get_phys_addr(&buf[j*PAGE_SIZE]);
			if (j <= i) {
				CHECK (pa != 0, "check if page is loaded");
				CHECK (buf[j*PAGE_SIZE] == (char) j, "check memory content");
			}
			else {
				CHECK (pa == 0, "check if page is not loaded");
			}
		}
	}
}

