#ifndef DEVICES_INTQ_H
#define DEVICES_INTQ_H

#include "threads/interrupt.h"
#include "threads/synch.h"

/* "interrupt queue"는 커널 스레드와 외부 interrupt handler가 공유하는
   원형 버퍼다.

   interrupt queue 함수들은 커널 스레드나 외부 interrupt handler에서
   호출할 수 있다. intq_init()을 제외하면 두 경우 모두 interrupt가
   꺼져 있어야 한다.

   interrupt queue는 "monitor" 구조를 가진다. 이 경우에는 일반적으로
   쓰는 threads/synch.h의 lock과 condition variable을 사용할 수 없다.
   그것들은 커널 스레드들끼리만 보호할 수 있고, interrupt handler로부터는
   보호할 수 없기 때문이다. */

/* queue 버퍼 크기, byte 단위. */
#define INTQ_BUFSIZE 64

/* byte를 저장하는 원형 queue. */
struct intq {
	/* 대기 중인 스레드들. */
	struct lock lock;           /* 한 번에 하나의 스레드만 기다릴 수 있다. */
	struct thread *not_full;    /* not-full 조건을 기다리는 스레드. */
	struct thread *not_empty;   /* not-empty 조건을 기다리는 스레드. */

	/* queue. */
	uint8_t buf[INTQ_BUFSIZE];  /* 버퍼. */
	int head;                   /* 새 데이터가 여기에 쓰인다. */
	int tail;                   /* 오래된 데이터가 여기서 읽힌다. */
};

void intq_init (struct intq *);
bool intq_empty (const struct intq *);
bool intq_full (const struct intq *);
uint8_t intq_getc (struct intq *);
void intq_putc (struct intq *, uint8_t);

#endif /* devices/intq.h */
