/* vm.c: 가상 메모리 객체를 위한 일반 인터페이스. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* 각 하위 시스템의 초기화 코드를 호출해 가상 메모리 하위 시스템을 초기화한다. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* 프로젝트 4용. */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* 위쪽 줄은 수정하지 말 것. */
	/* TODO: 여기에 구현을 작성한다. */
}

/* page의 타입을 얻는다. page가 초기화된 뒤 어떤 타입이 될지 알고 싶을 때
 * 유용한 함수다. 이 함수는 이미 완전히 구현되어 있다. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* 보조 함수. */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* initializer를 사용해 대기 중인 page 객체를 만든다. page를 만들고 싶다면
 * 직접 만들지 말고 이 함수나 `vm_alloc_page`를 통해 만들어라. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* upage가 이미 점유되어 있는지 확인한다. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: page를 만들고 VM 타입에 맞는 initializer를 가져온 뒤,
		 * TODO: uninit_new를 호출해 "uninit" page 구조체를 만든다.
		 * TODO: uninit_new 호출 뒤 field를 수정해야 한다. */

		/* TODO: page를 spt에 삽입한다. */
	}
err:
	return false;
}

/* spt에서 VA를 찾아 page를 반환한다. 오류가 나면 NULL을 반환한다. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: 이 함수를 채운다. */

	return page;
}

/* 검증과 함께 PAGE를 spt에 삽입한다. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: 이 함수를 채운다. */

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* evict될 struct frame을 얻는다. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: eviction 정책은 구현자가 정한다. */

	return victim;
}

/* page 하나를 evict하고 해당 frame을 반환한다.
 * 오류가 나면 NULL을 반환한다.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: victim을 swap out하고 evict된 frame을 반환한다. */

	return NULL;
}

/* palloc()으로 frame을 얻는다. 사용 가능한 page가 없으면 page를 evict하고
 * 그것을 반환한다. 이 함수는 항상 유효한 주소를 반환한다. 즉 user pool
 * 메모리가 가득 차면 frame을 evict해 사용 가능한 메모리 공간을 얻는다.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: 이 함수를 채운다. */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* 스택을 키운다. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* write-protected page에서 발생한 fault를 처리한다. */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* 성공하면 true를 반환한다. */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: fault를 검증한다. */
	/* TODO: 여기에 구현을 작성한다. */

	return vm_do_claim_page (page);
}

/* page를 해제한다.
 * 이 함수는 수정하지 말 것. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* VA에 할당된 page를 claim한다. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: 이 함수를 채운다. */

	return vm_do_claim_page (page);
}

/* PAGE를 claim하고 mmu를 설정한다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* link를 설정한다. */
	frame->page = page;
	page->frame = frame;

	/* TODO: page의 VA를 frame의 PA에 매핑하는 page table entry를 삽입한다. */

	return swap_in (page, frame->kva);
}

/* 새 supplemental page table을 초기화한다. */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
}

/* supplemental page table을 src에서 dst로 복사한다. */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* supplemental page table이 보유한 자원을 해제한다. */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: thread가 보유한 모든 supplemental_page_table을 파괴하고,
	 * TODO: 수정된 모든 내용을 저장소에 writeback한다. */
}
