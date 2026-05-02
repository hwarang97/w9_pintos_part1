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

	/*
	unsigned int SYS_CALL = f->R.rax;

	switch(SYS_CALL){

		case SYS_WRITE:

			int fd = f->R.rdi;
			const void *buffer = f->R.rsi;
			unsigned size = f->R.rdx;

			// 유효한 버퍼 주소 검사
			int is_valid_address = is_user_vaddr(buffer);

			// 유효한 주소, 콘솔 출력
			if (fd == stdout && is_valid_address) {

				// 가상 주소 매핑 mmu.c 파일에서 함수 이용
				// buffer 가상 주소를 물리 주소로 변환
				// 테이블을  생성해야할지, 기존걸 사용해야할지 모르겠음
				uint64_t* real_buffer = pml4_get_page(thread_current()->pml4, buffer);


				putbuf(real_buffer, size);
				f->R.rax = size; // 원래는 실제 적힌 사이즈를 반환해야하지만, 현재 테스트에서는 size를 반환하는걸로 만족
			}

			// 그 외
			else {
				f->R.rax = -1; // sentinel value (공통 규칙으로 나온것같음)
			}

			break;

		case SYS_EXIT:
			int status = f->R.rdi (음수가 지원되나?)
			f->R.rax = status
			thread_exit();
			break;
      
		default:
      //R.rax에 대한 예외처리 : 프로세스 종료, 에러 출력, rax에 반환값 -1 (그러나 rax가 uint64로 선언되었기에 가능여부 확인 필요)

	}
	*/
}
