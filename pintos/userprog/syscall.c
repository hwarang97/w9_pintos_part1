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

/* 시스템 콜.
 *
 * 이전에는 시스템 콜 서비스가 인터럽트 핸들러에서 처리되었다
 * (예: 리눅스의 int 0x80). 하지만 x86-64에서는 제조사가 시스템 콜을
 * 요청하기 위한 효율적인 경로인 `syscall` 명령어를 제공한다.
 *
 * syscall 명령어는 Model Specific Register(MSR)에서 값을 읽어서 동작한다.
 * 자세한 내용은 매뉴얼을 참고한다. */

#define MSR_STAR 0xc0000081			/* 세그먼트 선택자 MSR */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL 대상 주소 */
#define MSR_SYSCALL_MASK 0xc0000084 /* eflags용 마스크 */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* syscall_entry가 유저 영역 스택을 커널 모드 스택으로 바꾸기 전까지는
	 * 인터럽트 서비스 루틴이 어떤 인터럽트도 처리하면 안 된다.
	 * 따라서 FLAG_FL을 마스킹한다. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}
/*
	buffer 데이터 유효성 검사 의사 코드
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
	switch(f->R.rax){
		case SYS_WRITE:{
	        int fd=f->R.rdi;
		    const void *buffer = (const void *)f->R.rsi;
		    unsigned size = f->R.rdx;

		    if(fd == 1){
			    putbuf((const char *) buffer, size);
			    f->R.rax = size;
		    }else{
			    f->R.rax = -1;
		    }
		    break;
	}
		case SYS_EXIT:{
			int status = f->R.rdi;
            printf("exit status = %d\n", status);
		    thread_exit();
		    break;
	}
	default:
	    break;
	
}
}
	/*
	intr_frame에서 값을 가져오기
	rax: 시스템 콜 번호
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

			// 유저 영역 메모리 주소 확인 (KERN_BASE)
			int is_valid_address = is_user_vaddr(buffer);

			// 매핑된 주소인지 확인 (TODO: 페이지 테이블 조건 구체화 필요)
			is_mapped_address = (pml4_get_page(thread_current()->pml4, buffer) == NULL)

			// 유효한 주소, 콘솔 출력
			if (fd == stdout && is_valid_address && is_mapped_address) {
				putbuf(buffer, size);
				f->R.rax = size; // 원래는 실제 적힌 사이즈를 반환해야하지만, 현재 테스트에서는 size를 반환하는걸로 만족
			}

			// 실패
			else {
				f->R.rax = -1;
			}

			break;

		case SYS_EXIT:
			int status = f->R.rdi (음수 사용 가능여부 확인 필요)
			f->R.rax = status
			thread_exit();
			break;

		default:
	  //R.rax에 대한 예외처리 : 프로세스 종료, 에러 출력, rax에 반환값 -1 (그러나 rax가 uint64로 선언되었기에 가능여부 확인 필요)

	}
	*/
