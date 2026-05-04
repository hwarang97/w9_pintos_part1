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


addr_compare(struct intr_frame *f)
{ 
	if ( (f->R.rdi == stdout) && (is_user_vaddr(f)))
	return true;
	else
	return -1;
}
// int is_vaddr_valid(const char * buffer, unsigned size){
// 	for(buffer, buffer<=buffer+size-1, buffer += PGSIZE) {
// 		if (pml4_get_page(thread_current()->pml4, buffer)){
// 			continue;
// 		} else {
// 			return -1;
// 		}
		
// 	}
// 	return 0;
// }

/* 메인 시스템 콜 인터페이스 */
void syscall_handler(struct intr_frame *f) //이 UNUSED 부분 의논
{
	/*
	intr_frame에서 값을 가져오기
	rax: syscall number
	rdi: 1 fd값
	rsi: 2 
	rdx: 3
	r10: 4
	r8:  5
	r9:  6
	*/

	unsigned int SYS_CALL = f->R.rax;

	//syscall이 들어왔을때, 해당 주소가 현재 프로세스의 PML4 테이블에 올바르게 매핑되어 있는지를 추적

	switch(SYS_CALL){

		case SYS_WRITE:

			int fd = f->R.rdi;
			const void *buffer = f->R.rsi; //출력할 실제 문자열 데이터가 담긴 메모리의 주소
			unsigned size = f->R.rdx; //출력할 데이터의 총 크기 수


			int is_valid = 1;
	

			if (size == 0) { //size값 검사. 0일때는 아무 것도 하지 않음
				f->R.rax = size;
				break;
			}

			if (buffer == NULL)
			{
				thread_exit();
			} 

			char *start_page = pg_round_down((char *)buffer);
			char *end_page = pg_round_up((char *)buffer + size - 1);
			for (char *page = *start_page; page <= end_page; page += PGSIZE)
			{
				if (!is_user_vaddr(page) || (pml4_get_page(thread_current()-> pml4, page) == NULL))
				{
					is_valid = 0;
					break;
				}
			}

			if (fd == stdout && is_valid)
			{
				putbuf(buffer, size);
				f->R.rax = size;
			}


			// // buffer값 유효성 검사 및 처리
			// int is_valid_address = is_vaddr_valid(buffer, size);


			// // 유효한 주소, 콘솔 출력
			
			// if (fd == stdout && is_user_vaddr(buffer) && is_valid_address == 0) {
				
			// 	putbuf((char *)buffer, size); //
			// 	f->R.rax = size; // 원래는 실제 적힌 사이즈를 반환해야하지만, 현재 테스트에서는 size를 반환하는걸로 만족
			// }

			// 실패
			else {
				f->R.rax = -1; // sentinel value (음수 사용 가능여부 확인 필요)
			}

			break;


		case SYS_EXIT:
			int exit_status = f->R.rdi; 
			
			thread_current()->exit_status = exit_status;
			thread_exit();
			break; //break을 걸 필요가 있나?

		default:
			thread_exit();
      //R.rax에 대한 예외처리 : 프로세스 종료, 에러 출력, rax에 반환값 -1 (그러나 rax가 uint64로 선언되었기에 가능여부 확인 필요)

	}
}
