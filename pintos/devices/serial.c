#include "devices/serial.h"
#include <debug.h>
#include "devices/input.h"
#include "devices/intq.h"
#include "devices/timer.h"
#include "threads/io.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* PC에서 사용하는 16550A UART의 register 정의.
   16550A에는 여기 보이는 것보다 더 많은 기능이 있지만, 필요한 것은 이것뿐이다.

   hardware 정보는 [PC16650D]를 참고한다. */

/* 첫 번째 serial port의 I/O port base address. */
#define IO_BASE 0x3f8

/* DLAB=0 register. */
#define RBR_REG (IO_BASE + 0)   /* receiver buffer register(read-only). */
#define THR_REG (IO_BASE + 0)   /* transmitter holding register(write-only). */
#define IER_REG (IO_BASE + 1)   /* interrupt enable register. */

/* DLAB=1 register. */
#define LS_REG (IO_BASE + 0)    /* divisor latch(LSB). */
#define MS_REG (IO_BASE + 1)    /* divisor latch(MSB). */

/* DLAB에 영향받지 않는 register. */
#define IIR_REG (IO_BASE + 2)   /* interrupt identification register(read-only). */
#define FCR_REG (IO_BASE + 2)   /* FIFO control register(write-only). */
#define LCR_REG (IO_BASE + 3)   /* line control register. */
#define MCR_REG (IO_BASE + 4)   /* MODEM control register. */
#define LSR_REG (IO_BASE + 5)   /* line status register(read-only). */

/* interrupt enable register bit. */
#define IER_RECV 0x01           /* data를 받으면 interrupt. */
#define IER_XMIT 0x02           /* transmit이 끝나면 interrupt. */

/* line control register bit. */
#define LCR_N81 0x03            /* parity 없음, data bit 8개, stop bit 1개. */
#define LCR_DLAB 0x80           /* divisor latch access bit(DLAB). */

/* MODEM control register. */
#define MCR_OUT2 0x08           /* output line 2. */

/* line status register. */
#define LSR_DR 0x01             /* data ready: 받은 data byte가 RBR에 있다. */
#define LSR_THRE 0x20           /* THR empty. */

/* transmission mode. */
static enum { UNINIT, POLL, QUEUE } mode;

/* 전송할 data. */
static struct intq txq;

static void set_serial (int bps);
static void putc_poll (uint8_t);
static void write_ier (void);
static intr_handler_func serial_interrupt;

/* serial port device를 polling mode로 초기화한다.
   polling mode는 serial port가 비기 전까지 busy-wait한 뒤 쓴다.
   느리지만 interrupt가 초기화되기 전에는 이것이 할 수 있는 전부다. */
static void
init_poll (void) {
	ASSERT (mode == UNINIT);
	outb (IER_REG, 0);                    /* 모든 interrupt를 끈다. */
	outb (FCR_REG, 0);                    /* FIFO를 비활성화한다. */
	set_serial (115200);                  /* 115.2 kbps, N-8-1. */
	outb (MCR_REG, MCR_OUT2);             /* interrupt 활성화에 필요하다. */
	intq_init (&txq);
	mode = POLL;
}

/* serial port device를 queue 기반 interrupt-driven I/O로 초기화한다.
   interrupt-driven I/O를 쓰면 serial device가 준비될 때까지 CPU 시간을
   낭비하지 않는다. */
void
serial_init_queue (void) {
	enum intr_level old_level;

	if (mode == UNINIT)
		init_poll ();
	ASSERT (mode == POLL);

	intr_register_ext (0x20 + 4, serial_interrupt, "serial");
	mode = QUEUE;
	old_level = intr_disable ();
	write_ier ();
	intr_set_level (old_level);
}

/* BYTE를 serial port로 보낸다. */
void
serial_putc (uint8_t byte) {
	enum intr_level old_level = intr_disable ();

	if (mode != QUEUE) {
		/* 아직 interrupt-driven I/O가 설정되지 않았다면 단순 polling으로
		   byte를 전송한다. */
		if (mode == UNINIT)
			init_poll ();
		putc_poll (byte);
	} else {
		/* 그렇지 않으면 byte를 queue에 넣고 interrupt enable register를 갱신한다. */
		if (old_level == INTR_OFF && intq_full (&txq)) {
			/* interrupt가 꺼져 있고 transmit queue가 가득 찼다.
			   queue가 빌 때까지 기다리려면 interrupt를 다시 켜야 한다.
			   이는 좋지 않으므로 대신 polling으로 문자를 보낸다. */
			putc_poll (intq_getc (&txq));
		}

		intq_putc (&txq, byte);
		write_ier ();
	}

	intr_set_level (old_level);
}

/* serial buffer에 있는 것을 polling mode로 port 밖으로 flush한다. */
void
serial_flush (void) {
	enum intr_level old_level = intr_disable ();
	while (!intq_empty (&txq))
		putc_poll (intq_getc (&txq));
	intr_set_level (old_level);
}

/* input buffer가 얼마나 찼는지가 바뀌었을 수 있다. receive interrupt를 막아야
   하는지 다시 판단한다. 문자가 buffer에 추가되거나 제거될 때 input buffer
   routine이 호출한다. */
void
serial_notify (void) {
	ASSERT (intr_get_level () == INTR_OFF);
	if (mode == QUEUE)
		write_ier ();
}

/* serial port를 초당 BPS bit로 설정한다. */
static void
set_serial (int bps) {
	int base_rate = 1843200 / 16;         /* 16550A의 base rate, Hz 단위. */
	uint16_t divisor = base_rate / bps;   /* clock rate divisor. */

	ASSERT (bps >= 300 && bps <= 115200);

	/* DLAB를 활성화한다. */
	outb (LCR_REG, LCR_N81 | LCR_DLAB);

	/* data rate를 설정한다. */
	outb (LS_REG, divisor & 0xff);
	outb (MS_REG, divisor >> 8);

	/* DLAB를 reset한다. */
	outb (LCR_REG, LCR_N81);
}

/* interrupt enable register를 갱신한다. */
static void
write_ier (void) {
	uint8_t ier = 0;

	ASSERT (intr_get_level () == INTR_OFF);

	/* 전송할 문자가 있으면 transmit interrupt를 활성화한다. */
	if (!intq_empty (&txq))
		ier |= IER_XMIT;

	/* 받을 문자를 저장할 공간이 있으면 receive interrupt를 활성화한다. */
	if (!input_full ())
		ier |= IER_RECV;

	outb (IER_REG, ier);
}

/* serial port가 준비될 때까지 polling한 뒤 BYTE를 전송한다. */
static void
putc_poll (uint8_t byte) {
	ASSERT (intr_get_level () == INTR_OFF);

	while ((inb (LSR_REG) & LSR_THRE) == 0)
		continue;
	outb (THR_REG, byte);
}

/* serial interrupt handler. */
static void
serial_interrupt (struct intr_frame *f UNUSED) {
	/* UART의 interrupt를 확인한다. 이것이 없으면 QEMU에서 실행할 때 가끔
	   interrupt를 놓칠 수 있다. */
	inb (IIR_REG);

	/* byte를 받을 공간이 있고 hardware에 받을 byte가 있는 동안 byte를 받는다. */
	while (!input_full () && (inb (LSR_REG) & LSR_DR) != 0)
		input_putc (inb (RBR_REG));

	/* 전송할 byte가 있고 hardware가 전송용 byte를 받을 준비가 된 동안 byte를 전송한다. */
	while (!intq_empty (&txq) && (inb (LSR_REG) & LSR_THRE) != 0)
		outb (THR_REG, intq_getc (&txq));

	/* queue 상태에 따라 interrupt enable register를 갱신한다. */
	write_ier ();
}
