#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* 스레드 생명 주기의 상태들. */
enum thread_status
{
	THREAD_RUNNING, /* 실행 중인 스레드. */
	THREAD_READY,	/* 실행 중은 아니지만 실행 준비가 된 상태. */
	THREAD_BLOCKED, /* 어떤 이벤트가 발생하기를 기다리는 상태. */
	THREAD_DYING	/* 곧 파괴될 상태. */
};

/* 스레드 식별자 타입.
   원하는 타입으로 다시 정의해도 된다. */
typedef int tid_t;
#define TID_ERROR ((tid_t) - 1) /* tid_t의 오류 값. */

/* 스레드 우선순위. */
#define PRI_MIN 0	   /* 가장 낮은 우선순위. */
#define PRI_DEFAULT 31 /* 기본 우선순위. */
#define PRI_MAX 63	   /* 가장 높은 우선순위. */

/* 커널 스레드 또는 유저 프로세스.
 *
 * 각 thread 구조체는 자기 자신의 4 kB 페이지에 저장된다.
 * thread 구조체 자체는 페이지의 가장 아래쪽, 즉 offset 0에 놓인다.
 * 나머지 공간은 스레드의 커널 스택으로 예약되며,
 * 이 스택은 페이지의 맨 위, 즉 offset 4 kB에서 아래쪽으로 자란다.
 * 그림으로 보면 다음과 같다.
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * 여기서 얻을 수 있는 결론은 두 가지다.
 *
 *    1. 첫째, `struct thread'가 너무 커지면 안 된다.
 *       너무 커지면 커널 스택이 사용할 공간이 부족해진다.
 *       기본 `struct thread'는 몇 바이트에 불과하다.
 *       아마 1 kB보다 훨씬 작게 유지되어야 한다.
 *
 *    2. 둘째, 커널 스택이 너무 커지면 안 된다.
 *       스택이 overflow되면 스레드 상태를 망가뜨린다.
 *       따라서 커널 함수는 큰 구조체나 배열을 non-static 지역 변수로
 *       할당하면 안 된다. 대신 malloc()이나 palloc_get_page()를 이용한
 *       동적 할당을 사용한다.
 *
 * 두 문제 중 하나가 발생했을 때 처음 보이는 증상은 아마
 * thread_current()의 assertion 실패일 것이다. 이 함수는 실행 중인
 * 스레드의 `struct thread' 안에 있는 `magic' 멤버가 THREAD_MAGIC으로
 * 설정되어 있는지 확인한다. 스택 overflow는 보통 이 값을 바꾸어서
 * assertion을 발생시킨다. */
/* `elem' 멤버는 두 가지 용도를 가진다. thread.c의 run queue 원소가 될 수도 있고,
 * synch.c의 semaphore wait list 원소가 될 수도 있다.
 * 이 두 용도로 사용할 수 있는 이유는 두 상황이 동시에 일어나지 않기 때문이다.
 * ready 상태인 스레드만 run queue에 있고, blocked 상태인 스레드만
 * semaphore wait list에 있다. */
struct thread
{
	/* thread.c가 소유한다. */
	tid_t tid;				   /* 스레드 식별자. */
	enum thread_status status; /* 스레드 상태. */
	char name[16];			   /* 디버깅용 이름. */
	int priority;			   /* 우선순위. */
	int64_t wake_tick;		   // thread에 깨어날 시간 저장공간 생성
	/* thread.c와 synch.c가 함께 사용한다. */
	struct list_elem elem, sleep_elem, all_elem; // sleep_list 리스트 연결 정보 저장공간 생성       /* 리스트 원소. */
	int nice;
	int recent_cpu;
#ifdef USERPROG
	/* userprog/process.c가 소유한다. */
	uint64_t *pml4;	 /* load 함수에서 할당된다 */
	int exit_status; /* 종료 상태 변수 */ 
#endif
#ifdef VM
	/* 스레드가 소유한 전체 가상 메모리를 위한 테이블. */
	struct supplemental_page_table spt;
#endif

	/* thread.c가 소유한다. */
	struct intr_frame tf; /* 전환에 필요한 정보. */
	unsigned magic;		  /* 스택 overflow를 감지한다. */
};

/* false이면 기본값으로 round-robin scheduler를 사용한다.
   true이면 multi-level feedback queue scheduler를 사용한다.
   커널 명령줄 옵션 "-o mlfqs"로 제어된다. */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);

#endif /* threads/thread.h */

bool thread_priority_vs(const struct list_elem *a, // 어디에서든 사용할 수 있게 헤더에 추가 타입 static제거
						const struct list_elem *b,
						void *aux UNUSED);
