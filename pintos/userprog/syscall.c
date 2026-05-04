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
 * 예전에는 시스템 콜 서비스가 interrupt handler에서 처리되었다.
 * 예를 들어 Linux의 int 0x80이 있다. 하지만 x86-64에서는 제조사가
 * 시스템 콜을 요청하는 효율적인 경로인 `syscall` 명령어를 제공한다.
 *
 * syscall 명령어는 Model Specific Register(MSR)의 값을 읽어서 동작한다.
 * 자세한 내용은 매뉴얼을 참고한다. */

#define MSR_STAR 0xc0000081			/* 세그먼트 selector MSR */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL 대상 주소 */
#define MSR_SYSCALL_MASK 0xc0000084 /* eflags용 마스크 */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* syscall_entry가 유저 영역 스택을 커널 모드 스택으로 바꾸기 전까지는
	 * interrupt service routine이 어떤 interrupt도 처리하면 안 된다.
	 * 따라서 FLAG_FL을 마스킹한다. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

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
buffer data 유효성 검사 의사 코드
addr_compare(ptr)
{ 
//인자값 확인 필요
if ( (f->R.rdi == stdout) && (is_user_vaddr(ptr)) && (pml4_get_page()) )
	return ture
else 안맞을 때 
	return -1;
}
*/
bool addr_compare(struct intr_frame *f)
{
if( (f->R.rdi == 1) && (is_user_vaddr((void *)f->R.rsi)) && (f->R.rsi != NULL) )
	return true;
else
	return false;
}

/* 메인 시스템 콜 인터페이스 */
void syscall_handler(struct intr_frame *f )
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

	switch(SYS_CALL){

		case SYS_WRITE:
			//레지스터 변수
			int fd = f->R.rdi;
			const void *buffer = f->R.rsi;
			unsigned size = f->R.rdx;
			void *kernel_addr = NULL;

			// 유효한 버퍼 주소 검사
			int is_valid_address = is_user_vaddr(buffer);
			
			if (size == 0){
				f->R.rax = size;
				break;
			}
			if(buffer == NULL){
				thread_exit();
			}
			
			// 유효성 검사 후 uaddr을 kaddr로 변경
			if(addr_compare(f)){
				kernel_addr = pml4_get_page(thread_current() -> pml4, buffer);
			}
			if(kernel_addr != NULL){	
				putbuf((char*) buffer, size);
						f->R.rax = size; // 원래는 실제 적힌 사이즈를 반환해야하지만, 현재 테스트에서는 size를 반환하는걸로 만족
			}

			// 실패
			else {
				f->R.rax = -1; // sentinel value (음수 사용 가능여부 확인 필요)
			} // 버퍼 NULL일때는 스레드 종료해야함
			break;

		case SYS_EXIT:
			int status = f->R.rdi; //(음수 사용 가능여부 확인 필요)
			printf ("%s: exit(%d)\n", thread_name(), status);
			thread_exit();
			break;
      
		default:
      //R.rax에 대한 예외처리 : 프로세스 종료, 에러 출력, rax에 반환값 -1 (그러나 rax가 uint64로 선언되었기에 가능여부 확인 필요)
		}}