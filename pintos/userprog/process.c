#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name,char *arg_line, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);

/* 첫 유저 프로세스와 다른 프로세스에서 공통으로 쓰는 프로세스 초기화 함수. */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* 파일 이름에서 불러온 첫 번째 유저 프로그램을 시작한다.
 * 새 스레드는 이 함수가 반환되기 전에 스케줄될 수 있고,
 * 심지어 종료될 수도 있다. 새 스레드의 번호를 반환하며,
 * 스레드를 만들 수 없으면 스레드 오류 값을 반환한다.
 * 이 함수는 반드시 한 번만 호출되어야 한다. */
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;

	/* 파일 이름의 복사본을 만든다.
	 * 그렇지 않으면 호출자와 적재 함수 사이에 경쟁 상태가 생길 수 있다. */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

	/* 파일 이름을 실행할 새 스레드를 만든다. */
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return tid;
}

/* 첫 번째 유저 프로세스를 실행하는 스레드 함수. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* 현재 프로세스를 주어진 이름으로 복제한다. 새 프로세스의 스레드 번호를 반환하며,
 * 스레드를 만들 수 없으면 스레드 오류 값을 반환한다. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* 현재 스레드를 새 스레드로 복제한다. */
	return thread_create (name,
			PRI_DEFAULT, __do_fork, thread_current ());
}

#ifndef VM
/* 이 함수를 페이지 테이블 순회 함수에 넘겨 부모의 주소 공간을 복제한다.
 * 이 코드는 프로젝트 2에서만 사용된다. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. 할 일: 부모 페이지가 커널 페이지라면 즉시 반환한다. */

	/* 2. 부모의 페이지 맵 레벨 4에서 가상 주소를 해석한다. */
	parent_page = pml4_get_page (parent->pml4, va);

	/* 3. 할 일: 자식을 위한 새 유저 페이지를 할당하고 결과를
	 *    새 페이지에 저장한다. */

	/* 4. 할 일: 부모의 페이지를 새 페이지로 복사하고,
	 *    부모 페이지가 쓰기 가능한지 확인한다. 결과에 따라
	 *    쓰기 가능 값을 설정한다. */

	/* 5. 가상 주소에 있는 자식의 페이지 테이블에 새 페이지를 쓰기 가능
	 *    권한으로 추가한다. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. 할 일: 페이지 삽입에 실패하면 오류 처리를 한다. */
	}
	return true;
}
#endif

/* 부모의 실행 문맥을 복사하는 스레드 함수.
 * 힌트) 부모의 트랩 프레임은 프로세스의 유저 영역 문맥을 가지고 있지 않다.
 *       따라서 프로세스 복제 함수의 두 번째 인자를 이 함수로 넘겨야 한다. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* 할 일: 부모 인터럽트 프레임을 어떻게든 전달한다. 즉 복제 호출의 인터럽트 프레임이다. */
	struct intr_frame *parent_if;
	bool succ = true;

	/* 1. 중앙 처리 장치 문맥을 지역 스택으로 읽어온다. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));

	/* 2. 페이지 테이블을 복제한다. */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* 할 일: 여기에 코드를 작성한다.
	 * 힌트) 파일 객체를 복제하려면 파일 관련 헤더의 파일 복제 함수를 사용한다.
	 * 이 함수가 부모의 자원을 성공적으로 복제하기 전까지
	 * 부모는 복제 호출에서 반환되면 안 된다. */

	process_init ();

	/* 마지막으로 새로 만든 프로세스로 전환한다. */
	if (succ)
		do_iret (&if_);
error:
	thread_exit ();
}

/* 현재 실행 문맥을 파일 이름으로 전환한다.
 * 실패하면 -1을 반환한다. */
int
process_exec (void *f_name) {
	char *file_name = f_name;
	char *arg_copy = palloc_get_page(0); //문자열을 복사할 메모리 할당

	if(arg_copy == NULL){   //해당 메모리가 NULL이라면 기존에 전달받은 file_name을 해제
		palloc_free_page(file_name);
		return -1;  //실패 반환
	}

	strlcpy(arg_copy, file_name, PGSIZE);  //문자열을 arg_copy에 복사한다
	
	char *save_ptr; //어디까지 잘랐는지 기억하는 포인터 변수
	char *exec_name = strtok_r(file_name, " ", &save_ptr); //strtok_r로 공백 기준으로 문자열을 잘라 첫 번째 단어만 반환
	bool success;

	/* 스레드 구조체 안의 인터럽트 프레임은 사용할 수 없다.
	 * 현재 스레드가 다시 스케줄될 때 실행 정보가 그 멤버에 저장되기 때문이다. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* 먼저 현재 문맥을 정리한다. */
	process_cleanup ();

	/* 그 다음 바이너리를 불러온다. */
	success = load (exec_name, arg_copy, &_if);


	/* 적재에 실패하면 종료한다. */
	palloc_free_page(arg_copy);
	palloc_free_page (file_name);
	if (!success)
		return -1;

	/* 전환된 프로세스를 시작한다. */
	do_iret (&_if);
	NOT_REACHED ();
}


/* 스레드 번호가 종료될 때까지 기다리고 종료 상태를 반환한다.
 * 커널에 의해 종료된 경우, 즉 예외 때문에 죽은 경우 -1을 반환한다.
 * 스레드 번호가 유효하지 않거나 호출한 프로세스의 자식이 아니거나,
 * 해당 스레드 번호에 대해 대기 함수가 이미 성공적으로 호출된 적이 있으면
 * 기다리지 않고 즉시 -1을 반환한다.
 *
 * 이 함수는 문제 2-2에서 구현된다. 현재는 아무 일도 하지 않는다. */
int
process_wait (tid_t child_tid UNUSED) {
	/* 주의: 첫 유저 프로세스를 기다리는 지점에서 핀토스가 종료된다.
	 * 대기 함수를 구현하기 전에는 여기에 무한 루프를
	 * 추가하는 것을 권장한다. */
	while(1){
	}
	return -1;
}

/* 프로세스를 종료한다. 이 함수는 스레드 종료 함수에서 호출된다. */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* 할 일: 여기에 코드를 작성한다.
	 * 프로세스 종료 메시지를 구현한다.
	 * 프로젝트 2의 프로세스 종료 설명 문서를 참고한다.
	 * 여기에서 프로세스 자원 정리를 구현하는 것을 권장한다. */
	

	process_cleanup ();
}

/* 현재 프로세스의 자원을 해제한다. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* 현재 프로세스의 페이지 디렉터리를 파괴하고
	 * 커널 전용 페이지 디렉터리로 되돌아간다. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* 여기서는 순서가 매우 중요하다. 페이지 디렉터리를 전환하기 전에
		 * 현재 스레드의 페이지 디렉터리 포인터를 비워야 한다. 그래야 타이머 인터럽트가
		 * 프로세스 페이지 디렉터리로 다시 전환하지 못한다.
		 * 프로세스의 페이지 디렉터리를 파괴하기 전에 기본 페이지 디렉터리를
		 * 활성화해야 한다. 그렇지 않으면 현재 활성 페이지 디렉터리가
		 * 이미 해제되고 지워진 것이 될 수 있다. */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* 다음 스레드에서 유저 코드를 실행할 수 있도록 중앙 처리 장치를 설정한다.
 * 이 함수는 모든 문맥 전환 때 호출된다. */
void
process_activate (struct thread *next) {
	/* 스레드의 페이지 테이블을 활성화한다. */
	pml4_activate (next->pml4);

	/* 인터럽트 처리에 사용할 스레드의 커널 스택을 설정한다. */
	tss_update (next);
}

/* 실행 파일 바이너리를 불러온다. 아래 정의들은 실행 파일 형식 명세에서
 * 거의 그대로 가져온 것이다. */

/* 실행 파일 형식 종류. */
#define EI_NIDENT 16

#define PT_NULL    0            /* 무시한다. */
#define PT_LOAD    1            /* 적재 가능한 세그먼트. */
#define PT_DYNAMIC 2            /* 동적 링킹 정보. */
#define PT_INTERP  3            /* 동적 로더 이름. */
#define PT_NOTE    4            /* 보조 정보. */
#define PT_SHLIB   5            /* 예약됨. */
#define PT_PHDR    6            /* 프로그램 헤더 테이블. */
#define PT_STACK   0x6474e551   /* 스택 세그먼트. */

#define PF_X 1          /* 실행 가능. */
#define PF_W 2          /* 쓰기 가능. */
#define PF_R 4          /* 읽기 가능. */

/* 실행 파일 헤더. 실행 파일 형식 명세를 참고한다.
 * 실행 파일 바이너리의 맨 앞에 위치한다. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* 축약 이름 */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* 파일 이름의 실행 파일을 현재 스레드에 불러온다.
 * 실행 파일의 시작 지점을 명령어 포인터에 저장하고,
 * 초기 스택 포인터를 스택 포인터에 저장한다.
 * 성공하면 참, 실패하면 거짓을 반환한다. */
static bool
load (const char *file_name,char *arg_line, struct intr_frame *if_) {  //*arg_line 인자 받아오기
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* 페이지 디렉터리를 할당하고 활성화한다. */
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* 실행 파일을 연다. */
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

	/* 실행 파일 헤더를 읽고 검증한다. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // 64비트 아키텍처
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* 프로그램 헤더들을 읽는다. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* 이 세그먼트는 무시한다. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* 일반 세그먼트.
						 * 앞부분은 디스크에서 읽고 나머지는 0으로 채운다. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* 전체가 0인 세그먼트.
						 * 디스크에서 아무것도 읽지 않는다. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* 스택을 설정한다. */
	if (!setup_stack (if_))
		goto done;

	/* 시작 주소. */
	if_->rip = ehdr.e_entry;
    
	char *argv[64]; //인자 최대 64개
	int argc = 0;  //문자열을 개수
	char *save_ptr; // 어디까지 나눴는지 기록
	char *token = strtok_r(arg_line, " ", &save_ptr); //strtok_r 인자 나누기
	while(token != NULL){  
		argv[argc] = token; //argv에 토큰값 넣어주기
		argc++; //인자 하나 추가

		token=strtok_r(NULL, " ", &save_ptr); //두 번째 호출부터는 첫 번째 인자로 NULL을 넘긴다
	}

	int j = argc - 1; //  j를 문자열 역순으로 넣을 수 있게 선언
	while(j>=0){
		size_t arg_len=strlen(argv[j])+1; //문자열 길이를 저장할 변수 타입은 strlen과 동일하게 size_t로 선언
		if_->rsp -= arg_len; //스택 포인터에서 문자열 길이만큼 빼고 다시 그 값을 스택 포인터에 넣는다.
		char *u_stack = (char *)if_->rsp; //스택에 빈주소 공간 연결
        char *arg_org = argv[j];  //argv[j]의 원래 주소 연결

        for (size_t k = 0; k < arg_len; k++) {  //문자열 하나씩 복사
            u_stack[k] = arg_org[k];
		} 
		argv[j] = (char *)if_->rsp; //argv[j]의 주소를 rsp의 내린 주소로 변경한다.
		j -= 1;
	}
	argv[argc] = NULL; //마지막 인자 NULL 대입
    if_->rsp -= if_->rsp % 8; //바이트 정렬

	int arg_idx = argc; //주소값 변수
	while(arg_idx >= 0){
		if_->rsp -= sizeof(char *);  //주소 공간만큼 rsp내리기
		*(char **)if_->rsp = argv[arg_idx]; //문자열의 시작 주소를 저장
		arg_idx -= 1; //arg_idx값 하나 감소
	}


	//rdi-> 함수의 첫 번째 인자를 담는 자리
	//rsi-> 함수의 두 번째 인자를 담는 자리
	char **argv_start = (char **)if_->rsp; // argv[0]이 저장된 위치 argv 배열의 시작 주소
	if_->rsp -= sizeof(char *); //
	*(char **)if_->rsp = NULL; //fake return address주소 NULL로 설정
	if_->R.rdi = argc; // main에 첫 번째 인자가 argc이기 때문에 argc전달
	if_->R.rsi = (uint64_t)argv_start; //main에 두 번째 인자 argv 전달
	
	success = true;

done:
	/* 적재 성공 여부와 관계없이 여기로 도달한다. */
	file_close (file);
	return success;
}


/* 프로그램 헤더가 파일 안의 유효하고 적재 가능한 세그먼트를 설명하는지 확인한다.
 * 그렇다면 참, 아니면 거짓을 반환한다. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* 파일 위치와 가상 주소는 같은 페이지 오프셋을 가져야 한다. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* 파일 위치는 파일 내부를 가리켜야 한다. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* 메모리 크기는 파일 크기 이상이어야 한다. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* 세그먼트는 비어 있으면 안 된다. */
	if (phdr->p_memsz == 0)
		return false;

	/* 가상 메모리 영역의 시작과 끝은 모두 유저 주소 공간 범위 안에 있어야 한다. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* 영역이 커널 가상 주소 공간을 가로질러 감기듯 넘어가면 안 된다. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* 0번 페이지 매핑을 금지한다.
	   0번 페이지를 매핑하는 것 자체가 좋지 않을 뿐 아니라,
	   이를 허용하면 유저 코드가 시스템 콜에 널 포인터를 넘겼을 때
	   메모리 복사 함수 등의 널 포인터 검사로 커널 패닉이 발생할 가능성이 크다. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* 문제가 없다. */
	return true;
}

#ifndef VM
/* 이 블록의 코드는 프로젝트 2 동안에만 사용된다.
 * 프로젝트 2 전체에서 사용할 함수를 구현하려면 조건부 컴파일 매크로 밖에 구현한다. */

/* 적재 보조 함수들. */
static bool install_page (void *upage, void *kpage, bool writable);

/* 파일의 오프셋에서 시작하는 세그먼트를 유저 페이지 주소에 불러온다.
 * 총 읽기 바이트 수 + 0 채움 바이트 수만큼의 가상 메모리를 다음과 같이 초기화한다.
 *
 * - 유저 페이지 주소부터 읽기 바이트 수만큼은 파일의 오프셋부터 읽어야 한다.
 *
 * - 유저 페이지 주소 + 읽기 바이트 수부터 0 채움 바이트 수만큼은 0으로 채워야 한다.
 *
 * 이 함수가 초기화한 페이지들은 쓰기 가능 값이 참이면 유저 프로세스가 쓸 수 있고,
 * 그렇지 않으면 읽기 전용이어야 한다.
 *
 * 성공하면 참을 반환하고, 메모리 할당 오류나 디스크 읽기 오류가 발생하면
 * 거짓을 반환한다. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* 이 페이지를 어떻게 채울지 계산한다.
		 * 파일에서 페이지 읽기 바이트 수만큼 읽고,
		 * 마지막 페이지 0 채움 바이트 수만큼은 0으로 채운다. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* 메모리 페이지를 얻는다. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* 이 페이지를 불러온다. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* 이 페이지를 프로세스의 주소 공간에 추가한다. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* 다음 위치로 이동한다. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* 유저 스택에 0으로 채운 페이지를 매핑해서 최소 스택을 만든다. */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* 유저 가상 주소에서 커널 가상 주소로 가는 매핑을 페이지 테이블에 추가한다.
 * 쓰기 가능 값이 참이면 유저 프로세스가 페이지를 수정할 수 있고,
 * 그렇지 않으면 읽기 전용이다.
 * 유저 가상 주소는 이미 매핑되어 있으면 안 된다.
 * 커널 가상 주소는 보통 페이지 할당 함수로 유저 풀에서 얻은 페이지여야 한다.
 * 성공하면 참을 반환하고, 유저 가상 주소가 이미 매핑되어 있거나 메모리 할당에
 * 실패하면 거짓을 반환한다. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* 해당 가상 주소에 이미 페이지가 없는지 확인한 뒤,
	 * 그 위치에 우리 페이지를 매핑한다. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* 여기부터의 코드는 프로젝트 3 이후에 사용된다.
 * 프로젝트 2에서만 사용할 함수를 구현하려면 위쪽 블록에 구현한다. */

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* 할 일: 파일에서 세그먼트를 불러온다. */
	/* 할 일: 가상 주소에서 첫 페이지 폴트가 발생했을 때 호출된다. */
	/* 할 일: 이 함수를 호출할 때 가상 주소를 사용할 수 있다. */
}

/* 파일의 오프셋에서 시작하는 세그먼트를 유저 페이지 주소에 불러온다.
 * 총 읽기 바이트 수 + 0 채움 바이트 수만큼의 가상 메모리를 다음과 같이 초기화한다.
 *
 * - 유저 페이지 주소부터 읽기 바이트 수만큼은 파일의 오프셋부터 읽어야 한다.
 *
 * - 유저 페이지 주소 + 읽기 바이트 수부터 0 채움 바이트 수만큼은 0으로 채워야 한다.
 *
 * 이 함수가 초기화한 페이지들은 쓰기 가능 값이 참이면 유저 프로세스가 쓸 수 있고,
 * 그렇지 않으면 읽기 전용이어야 한다.
 *
 * 성공하면 참을 반환하고, 메모리 할당 오류나 디스크 읽기 오류가 발생하면
 * 거짓을 반환한다. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* 이 페이지를 어떻게 채울지 계산한다.
		 * 파일에서 페이지 읽기 바이트 수만큼 읽고,
		 * 마지막 페이지 0 채움 바이트 수만큼은 0으로 채운다. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* 할 일: 지연 세그먼트 적재 함수에 정보를 넘기기 위한 보조 값을 설정한다. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;

		/* 다음 위치로 이동한다. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* 유저 스택에 스택용 페이지를 만든다. 성공하면 참을 반환한다. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* 할 일: 스택 바닥 주소에 스택을 매핑하고 즉시 페이지를 확보한다.
	 * 성공하면 그에 맞게 스택 포인터를 설정한다.
	 * 해당 페이지를 스택으로 표시해야 한다. */
	/* 할 일: 여기에 코드를 작성한다. */

	return success;
}
#endif /* 가상 메모리 */
