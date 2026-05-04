#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* 세그먼트 selector MSR */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL 대상 주소 */
#define MSR_SYSCALL_MASK 0xc0000084 /* eflags용 마스크 */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/*
buffer data 유효성 검사 의사 코드
addr_compare(ptr)
{
	//인자값 확인 필요
	if ( (f->R.rdi == stdout) && (is_user_vaddr(ptr)) && (pml4_get_page()) )
		return true
	else 안맞을 때
		return -1;
}
*/

/* 메인 시스템 콜 인터페이스 */
void syscall_handler(struct intr_frame *f UNUSED)
{
	/*
	intr_frame에서 값을 가져오기
	rax: syscall number
	rdi: 1
	rsi: 2
	rdx: 3
	r10: 4
	r8:  5
	r9:  6
	*/

	unsigned int SYS_CALL = f->R.rax;

	switch (SYS_CALL)
	{

	case SYS_WRITE:

		// 레지스터 변수
		int fd = f->R.rdi;
		const char *buffer = (const char *)f->R.rsi;
		unsigned size = f->R.rdx;

		// 조건 변수
		int is_valid = 1;

		// size == 0, 아무것도 하지 않아도 된다.
		if (size == 0)
		{
			f->R.rax = size;
			break;
		}

		// buffer NULL 체크
		if (buffer == NULL)
		{
			// 현재 스레드를 종료하면 된다
			thread_exit();
		}

		// buffer가 사용중인 페이지가 유저영역인지, 매핑되어있는 확인
		char *start_page = pg_round_down((char *)buffer);
		char *end_page = pg_round_down((char *)buffer + size - 1);
		for (char *page = start_page; page <= end_page; page += PGSIZE)
		{
			if (!is_user_vaddr(page) || (pml4_get_page(thread_current()->pml4, page) == NULL))
			{
				is_valid = 0;
				break;
			}
		}

		// 유효한 주소, 콘솔 출력
		if (fd == STDOUT_FILENO && is_valid)
		{
			putbuf(buffer, size);
			f->R.rax = size; // 원래는 실제 적힌 사이즈를 반환해야하지만, 현재 테스트에서는 size를 반환하는걸로 만족
		}

		// 실패
		else
		{
			f->R.rax = -1;
		}

		break;

	case SYS_EXIT:
		int exit_status = f->R.rdi;

		// 종료 상태 저장
		thread_current()->exit_status = exit_status;
		thread_exit();

	default:
		// R.rax에 대한 예외처리 : 프로세스 종료, 에러 출력, rax에 반환값 -1 (그러나 rax가 uint64로 선언되었기에 가능여부 확인 필요)
	}
}
