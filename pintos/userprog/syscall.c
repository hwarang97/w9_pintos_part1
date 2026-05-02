#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) { //intr_frame에 모든 정보가 다 들어있

	// intr_frame에서 값을 가져와야 한다. 
	// rax 시스템 콜 번호 rdi 파일 (stdin,stdout), rsi: 버퍼, rdx : 크기
	// if (rax == SYS_WRITE) :
	// 
	//

	

	
	switch(f->R.rax){
	// Case Write
	case SYS_WRITE:
		// 버퍼를 가리키는 주소값이 제대로 들어왔는지 검증. 근데 어떤 케이스? 
		// 유저 스택쪽을 잘 가리키고 있는지 검증.
		// 그래도 역참조하기 전엔 이 값이 정말 잘 가리키고 있는지를 알 수 없지 않나? 이 정도 쿠션으로 괜찮은가?
		// 역참조를 했다고 쳤을 때, 무얼 검증해야 하는지? is_user_vaddr(vaddr)
		
		// 페이지 레벨에 대하여 : 레벨4 가상주소 ->물리주소 위한 테이블. 제일 큼. 레벨 3: Page directory pointer 
		// 다단계 페이지 테이블...과 관련된 내용? 
		// 가상주소를 물리주소로 바꾸는 코드? vtop(vaddr.h에 존재)
		putbuf(f->R.rsi, f->R.rdx);
	
	case SYS_EXIT:

	//R.rax 전체에 대한 예외처리 : else일때 에러코드 및 exit?
	


	}
	// 지금 문제. printf가 Kernel로 요청을 보낸 상태. 어떻게? printf를 타고 가다 보면 write가 있는데, 이건 User 측에 있는 
	// write가 Kernel한테 이 내용 콘솔에 좀 써달라고 부탁을 한 상태.
	// 즉 여기서 할 일은 콘솔에 쓰는 거. 
	// 근데 내가 궁금한 점은, write가 syscall_handler로 어떻게 연결이 되었느냐? 타고 타고 들어가면 syscall이 끝인데.

	printf ("system call!\n");
	thread_exit ();
}
