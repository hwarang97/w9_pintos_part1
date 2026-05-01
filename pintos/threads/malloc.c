#include "threads/malloc.h"
#include <debug.h>
#include <list.h>
#include <round.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* malloc()의 단순한 구현.

   각 요청의 크기는 byte 단위로 2의 거듭제곱으로 올림되고, 그 크기의 block을
   관리하는 "descriptor"에 배정된다. descriptor는 free block list를 유지한다.
   free list가 비어 있지 않으면 그 block 중 하나를 사용해 요청을 만족시킨다.

   그렇지 않으면 page allocator에서 "arena"라고 부르는 새 memory page를 얻는다.
   사용할 수 있는 page가 없으면 malloc()은 null 포인터를 반환한다. 새 arena는
   block들로 나뉘고, 모든 block은 descriptor의 free list에 추가된다. 그런 뒤
   새 block 중 하나를 반환한다.

   block을 free할 때는 해당 descriptor의 free list에 추가한다. 그러나 그 block이
   있던 arena에 이제 사용 중인 block이 없다면, arena의 모든 block을 free list에서
   제거하고 arena를 page allocator에 돌려준다.

   이 방식으로는 2 kB보다 큰 block을 처리할 수 없다. descriptor와 함께 단일
   page에 넣기에는 너무 크기 때문이다. 그런 block은 page allocator로 연속된
   page를 할당하고, 할당된 block의 arena header 시작 부분에 할당 크기를 넣어
   처리한다. */

/* descriptor. */
struct desc {
	size_t block_size;          /* 각 원소의 크기, byte 단위. */
	size_t blocks_per_arena;    /* arena 안의 block 수. */
	struct list free_list;      /* free block list. */
	struct lock lock;           /* lock. */
};

/* arena 손상을 감지하기 위한 magic number. */
#define ARENA_MAGIC 0x9a548eed

/* arena. */
struct arena {
	unsigned magic;             /* 항상 ARENA_MAGIC으로 설정된다. */
	struct desc *desc;          /* 소유 descriptor, 큰 block이면 null. */
	size_t free_cnt;            /* free block 수, 큰 block에서는 page 수. */
};

/* free block. */
struct block {
	struct list_elem free_elem; /* free list 원소. */
};

/* descriptor 집합. */
static struct desc descs[10];   /* descriptor. */
static size_t desc_cnt;         /* descriptor 수. */

static struct arena *block_to_arena (struct block *);
static struct block *arena_to_block (struct arena *, size_t idx);

/* malloc() descriptor들을 초기화한다. */
void
malloc_init (void) {
	size_t block_size;

	for (block_size = 16; block_size < PGSIZE / 2; block_size *= 2) {
		struct desc *d = &descs[desc_cnt++];
		ASSERT (desc_cnt <= sizeof descs / sizeof *descs);
		d->block_size = block_size;
		d->blocks_per_arena = (PGSIZE - sizeof (struct arena)) / block_size;
		list_init (&d->free_list);
		lock_init (&d->lock);
	}
}

/* 최소 SIZE byte인 새 block을 얻어 반환한다.
   memory를 사용할 수 없으면 null 포인터를 반환한다. */
void *
malloc (size_t size) {
	struct desc *d;
	struct block *b;
	struct arena *a;

	/* 0 byte 요청에는 null 포인터가 충분하다. */
	if (size == 0)
		return NULL;

	/* SIZE-byte 요청을 만족하는 가장 작은 descriptor를 찾는다. */
	for (d = descs; d < descs + desc_cnt; d++)
		if (d->block_size >= size)
			break;
	if (d == descs + desc_cnt) {
		/* SIZE가 어떤 descriptor에도 너무 크다.
		   SIZE와 arena를 담을 만큼 충분한 page를 할당한다. */
		size_t page_cnt = DIV_ROUND_UP (size + sizeof *a, PGSIZE);
		a = palloc_get_multiple (0, page_cnt);
		if (a == NULL)
			return NULL;

		/* arena를 PAGE_CNT page짜리 큰 block으로 표시하도록 초기화하고 반환한다. */
		a->magic = ARENA_MAGIC;
		a->desc = NULL;
		a->free_cnt = page_cnt;
		return a + 1;
	}

	lock_acquire (&d->lock);

	/* free list가 비어 있으면 새 arena를 만든다. */
	if (list_empty (&d->free_list)) {
		size_t i;

		/* page를 할당한다. */
		a = palloc_get_page (0);
		if (a == NULL) {
			lock_release (&d->lock);
			return NULL;
		}

		/* arena를 초기화하고 그 block들을 free list에 추가한다. */
		a->magic = ARENA_MAGIC;
		a->desc = d;
		a->free_cnt = d->blocks_per_arena;
		for (i = 0; i < d->blocks_per_arena; i++) {
			struct block *b = arena_to_block (a, i);
			list_push_back (&d->free_list, &b->free_elem);
		}
	}

	/* free list에서 block을 얻어 반환한다. */
	b = list_entry (list_pop_front (&d->free_list), struct block, free_elem);
	a = block_to_arena (b);
	a->free_cnt--;
	lock_release (&d->lock);
	return b;
}

/* 0으로 초기화된 A 곱하기 B byte를 할당해 반환한다.
   memory를 사용할 수 없으면 null 포인터를 반환한다. */
void *
calloc (size_t a, size_t b) {
	void *p;
	size_t size;

	/* block size를 계산하고 size_t에 들어가는지 확인한다. */
	size = a * b;
	if (size < a || size < b)
		return NULL;

	/* memory를 할당하고 0으로 채운다. */
	p = malloc (size);
	if (p != NULL)
		memset (p, 0, size);

	return p;
}

/* BLOCK에 할당된 byte 수를 반환한다. */
static size_t
block_size (void *block) {
	struct block *b = block;
	struct arena *a = block_to_arena (b);
	struct desc *d = a->desc;

	return d != NULL ? d->block_size : PGSIZE * a->free_cnt - pg_ofs (block);
}

/* OLD_BLOCK을 NEW_SIZE byte로 resize하려고 시도한다. 이 과정에서 block이
   이동할 수 있다. 성공하면 새 block을 반환하고, 실패하면 null 포인터를 반환한다.
   OLD_BLOCK이 null인 호출은 malloc(NEW_SIZE)와 같다.
   NEW_SIZE가 0인 호출은 free(OLD_BLOCK)과 같다. */
void *
realloc (void *old_block, size_t new_size) {
	if (new_size == 0) {
		free (old_block);
		return NULL;
	} else {
		void *new_block = malloc (new_size);
		if (old_block != NULL && new_block != NULL) {
			size_t old_size = block_size (old_block);
			size_t min_size = new_size < old_size ? new_size : old_size;
			memcpy (new_block, old_block, min_size);
			free (old_block);
		}
		return new_block;
	}
}

/* block P를 해제한다. P는 이전에 malloc(), calloc(), realloc()으로 할당되었어야 한다. */
void
free (void *p) {
	if (p != NULL) {
		struct block *b = p;
		struct arena *a = block_to_arena (b);
		struct desc *d = a->desc;

		if (d != NULL) {
			/* 일반 block이다. 여기서 처리한다. */

#ifndef NDEBUG
			/* use-after-free bug 감지를 돕기 위해 block을 지운다. */
			memset (b, 0xcc, d->block_size);
#endif

			lock_acquire (&d->lock);

			/* block을 free list에 추가한다. */
			list_push_front (&d->free_list, &b->free_elem);

			/* arena가 이제 완전히 사용되지 않는다면 해제한다. */
			if (++a->free_cnt >= d->blocks_per_arena) {
				size_t i;

				ASSERT (a->free_cnt == d->blocks_per_arena);
				for (i = 0; i < d->blocks_per_arena; i++) {
					struct block *b = arena_to_block (a, i);
					list_remove (&b->free_elem);
				}
				palloc_free_page (a);
			}

			lock_release (&d->lock);
		} else {
			/* 큰 block이다. 해당 page들을 해제한다. */
			palloc_free_multiple (a, a->free_cnt);
			return;
		}
	}
}

/* block B가 들어 있는 arena를 반환한다. */
static struct arena *
block_to_arena (struct block *b) {
	struct arena *a = pg_round_down (b);

	/* arena가 유효한지 확인한다. */
	ASSERT (a != NULL);
	ASSERT (a->magic == ARENA_MAGIC);

	/* block이 arena에 맞게 제대로 정렬되어 있는지 확인한다. */
	ASSERT (a->desc == NULL
			|| (pg_ofs (b) - sizeof *a) % a->desc->block_size == 0);
	ASSERT (a->desc != NULL || pg_ofs (b) == sizeof *a);

	return a;
}

/* arena A 안의 (IDX - 1)번째 block을 반환한다. */
static struct block *
arena_to_block (struct arena *a, size_t idx) {
	ASSERT (a != NULL);
	ASSERT (a->magic == ARENA_MAGIC);
	ASSERT (idx < a->desc->blocks_per_arena);
	return (struct block *) ((uint8_t *) a
			+ sizeof *a
			+ idx * a->desc->block_size);
}
