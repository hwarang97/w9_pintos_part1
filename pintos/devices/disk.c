#include "devices/disk.h"
#include <ctype.h>
#include <debug.h>
#include <stdbool.h>
#include <stdio.h>
#include "devices/timer.h"
#include "threads/io.h"
#include "threads/interrupt.h"
#include "threads/synch.h"

/* 이 파일의 코드는 ATA(IDE) controller와의 interface다.
   [ATA-3]을 따르려고 시도한다. */

#define reg_data(CHANNEL) ((CHANNEL)->reg_base + 0)     /* data. */
#define reg_error(CHANNEL) ((CHANNEL)->reg_base + 1)    /* error. */
#define reg_nsect(CHANNEL) ((CHANNEL)->reg_base + 2)    /* sector count. */
#define reg_lbal(CHANNEL) ((CHANNEL)->reg_base + 3)     /* LBA 0:7. */
#define reg_lbam(CHANNEL) ((CHANNEL)->reg_base + 4)     /* LBA 15:8. */
#define reg_lbah(CHANNEL) ((CHANNEL)->reg_base + 5)     /* LBA 23:16. */
#define reg_device(CHANNEL) ((CHANNEL)->reg_base + 6)   /* device/LBA 27:24. */
#define reg_status(CHANNEL) ((CHANNEL)->reg_base + 7)   /* status(r/o). */
#define reg_command(CHANNEL) reg_status (CHANNEL)       /* command(w/o). */

/* ATA control block port address.
   non-legacy ATA controller를 지원한다면 충분히 유연하지 않겠지만,
   여기서 하는 일에는 괜찮다. */
#define reg_ctl(CHANNEL) ((CHANNEL)->reg_base + 0x206)  /* control(w/o). */
#define reg_alt_status(CHANNEL) reg_ctl (CHANNEL)       /* alt status(r/o). */

/* alternate status register bit. */
#define STA_BSY 0x80            /* busy. */
#define STA_DRDY 0x40           /* device ready. */
#define STA_DRQ 0x08            /* data request. */

/* control register bit. */
#define CTL_SRST 0x04           /* software reset. */

/* device register bit. */
#define DEV_MBS 0xa0            /* 반드시 설정되어야 한다. */
#define DEV_LBA 0x40            /* linear based addressing. */
#define DEV_DEV 0x10            /* device 선택: 0=master, 1=slave. */

/* command.
   더 많이 정의되어 있지만 여기서는 이 작은 부분집합만 사용한다. */
#define CMD_IDENTIFY_DEVICE 0xec        /* IDENTIFY DEVICE. */
#define CMD_READ_SECTOR_RETRY 0x20      /* READ SECTOR with retries. */
#define CMD_WRITE_SECTOR_RETRY 0x30     /* WRITE SECTOR with retries. */

/* ATA device. */
struct disk {
	char name[8];               /* 이름, 예: "hd0:1". */
	struct channel *channel;    /* disk가 속한 channel. */
	int dev_no;                 /* master 또는 slave용 device 0 또는 1. */

	bool is_ata;                /* 1=이 device는 ATA disk다. */
	disk_sector_t capacity;     /* 용량, sector 단위(is_ata일 때). */

	long long read_cnt;         /* 읽은 sector 수. */
	long long write_cnt;        /* 쓴 sector 수. */
};

/* ATA channel, 즉 controller.
   각 channel은 최대 두 disk를 제어할 수 있다. */
struct channel {
	char name[8];               /* 이름, 예: "hd0". */
	uint16_t reg_base;          /* base I/O port. */
	uint8_t irq;                /* 사용 중인 interrupt. */

	struct lock lock;           /* controller에 접근하려면 반드시 획득해야 한다. */
	bool expecting_interrupt;   /* interrupt가 예상되면 true, 어떤 interrupt든
								   spurious라면 false. */
	struct semaphore completion_wait;   /* interrupt handler가 up한다. */

	struct disk devices[2];     /* 이 channel의 device들. */
};

/* 표준 PC에 있는 두 "legacy" ATA channel을 지원한다. */
#define CHANNEL_CNT 2
static struct channel channels[CHANNEL_CNT];

static void reset_channel (struct channel *);
static bool check_device_type (struct disk *);
static void identify_ata_device (struct disk *);

static void select_sector (struct disk *, disk_sector_t);
static void issue_pio_command (struct channel *, uint8_t command);
static void input_sector (struct channel *, void *);
static void output_sector (struct channel *, const void *);

static void wait_until_idle (const struct disk *);
static bool wait_while_busy (const struct disk *);
static void select_device (const struct disk *);
static void select_device_wait (const struct disk *);

static void interrupt_handler (struct intr_frame *);

/* disk 하위 시스템을 초기화하고 disk를 감지한다. */
void
disk_init (void) {
	size_t chan_no;

	for (chan_no = 0; chan_no < CHANNEL_CNT; chan_no++) {
		struct channel *c = &channels[chan_no];
		int dev_no;

		/* channel을 초기화한다. */
		snprintf (c->name, sizeof c->name, "hd%zu", chan_no);
		switch (chan_no) {
			case 0:
				c->reg_base = 0x1f0;
				c->irq = 14 + 0x20;
				break;
			case 1:
				c->reg_base = 0x170;
				c->irq = 15 + 0x20;
				break;
			default:
				NOT_REACHED ();
		}
		lock_init (&c->lock);
		c->expecting_interrupt = false;
		sema_init (&c->completion_wait, 0);

		/* device들을 초기화한다. */
		for (dev_no = 0; dev_no < 2; dev_no++) {
			struct disk *d = &c->devices[dev_no];
			snprintf (d->name, sizeof d->name, "%s:%d", c->name, dev_no);
			d->channel = c;
			d->dev_no = dev_no;

			d->is_ata = false;
			d->capacity = 0;

			d->read_cnt = d->write_cnt = 0;
		}

		/* interrupt handler를 등록한다. */
		intr_register_ext (c->irq, interrupt_handler, c->name);

		/* hardware를 reset한다. */
		reset_channel (c);

		/* ATA hard disk와 다른 device를 구분한다. */
		if (check_device_type (&c->devices[0]))
			check_device_type (&c->devices[1]);

		/* hard disk 식별 정보를 읽는다. */
		for (dev_no = 0; dev_no < 2; dev_no++)
			if (c->devices[dev_no].is_ata)
				identify_ata_device (&c->devices[dev_no]);
	}

	/* 아래 줄은 수정하지 말 것. */
	register_disk_inspect_intr ();
}

/* disk 통계를 출력한다. */
void
disk_print_stats (void) {
	int chan_no;

	for (chan_no = 0; chan_no < CHANNEL_CNT; chan_no++) {
		int dev_no;

		for (dev_no = 0; dev_no < 2; dev_no++) {
			struct disk *d = disk_get (chan_no, dev_no);
			if (d != NULL && d->is_ata)
				printf ("%s: %lld reads, %lld writes\n",
						d->name, d->read_cnt, d->write_cnt);
		}
	}
}

/* CHAN_NO 번호의 channel 안에서 DEV_NO 번호의 disk를 반환한다.
   DEV_NO는 각각 master 또는 slave에 해당하는 0 또는 1이다.

   Pintos는 disk를 다음과 같이 사용한다:
0:0 - boot loader, command line 인자, operating system kernel
0:1 - file system
1:0 - scratch
1:1 - swap
*/
struct disk *
disk_get (int chan_no, int dev_no) {
	ASSERT (dev_no == 0 || dev_no == 1);

	if (chan_no < (int) CHANNEL_CNT) {
		struct disk *d = &channels[chan_no].devices[dev_no];
		if (d->is_ata)
			return d;
	}
	return NULL;
}

/* disk D의 크기를 DISK_SECTOR_SIZE-byte sector 단위로 반환한다. */
disk_sector_t
disk_size (struct disk *d) {
	ASSERT (d != NULL);

	return d->capacity;
}

/* disk D의 SEC_NO sector를 BUFFER로 읽는다. BUFFER에는 DISK_SECTOR_SIZE
   byte를 담을 공간이 있어야 한다.
   disk 접근은 내부에서 동기화하므로 외부 per-disk locking은 필요 없다. */
void
disk_read (struct disk *d, disk_sector_t sec_no, void *buffer) {
	struct channel *c;

	ASSERT (d != NULL);
	ASSERT (buffer != NULL);

	c = d->channel;
	lock_acquire (&c->lock);
	select_sector (d, sec_no);
	issue_pio_command (c, CMD_READ_SECTOR_RETRY);
	sema_down (&c->completion_wait);
	if (!wait_while_busy (d))
		PANIC ("%s: disk read failed, sector=%"PRDSNu, d->name, sec_no);
	input_sector (c, buffer);
	d->read_cnt++;
	lock_release (&c->lock);
}

/* BUFFER에서 disk D의 SEC_NO sector로 쓴다. BUFFER는 DISK_SECTOR_SIZE byte를
   담고 있어야 한다. disk가 data 수신을 acknowledge한 뒤 반환한다.
   disk 접근은 내부에서 동기화하므로 외부 per-disk locking은 필요 없다. */
void
disk_write (struct disk *d, disk_sector_t sec_no, const void *buffer) {
	struct channel *c;

	ASSERT (d != NULL);
	ASSERT (buffer != NULL);

	c = d->channel;
	lock_acquire (&c->lock);
	select_sector (d, sec_no);
	issue_pio_command (c, CMD_WRITE_SECTOR_RETRY);
	if (!wait_while_busy (d))
		PANIC ("%s: disk write failed, sector=%"PRDSNu, d->name, sec_no);
	output_sector (c, buffer);
	sema_down (&c->completion_wait);
	d->write_cnt++;
	lock_release (&c->lock);
}

/* disk 감지와 식별. */

static void print_ata_string (char *string, size_t size);

/* ATA channel을 reset하고 그 위에 존재하는 device들이 reset을 끝낼 때까지 기다린다. */
static void
reset_channel (struct channel *c) {
	bool present[2];
	int dev_no;

	/* ATA reset sequence는 어떤 device가 존재하는지에 따라 달라지므로,
	   먼저 device 존재 여부를 감지한다. */
	for (dev_no = 0; dev_no < 2; dev_no++) {
		struct disk *d = &c->devices[dev_no];

		select_device (d);

		outb (reg_nsect (c), 0x55);
		outb (reg_lbal (c), 0xaa);

		outb (reg_nsect (c), 0xaa);
		outb (reg_lbal (c), 0x55);

		outb (reg_nsect (c), 0x55);
		outb (reg_lbal (c), 0xaa);

		present[dev_no] = (inb (reg_nsect (c)) == 0x55
				&& inb (reg_lbal (c)) == 0xaa);
	}

	/* soft reset sequence를 발생시킨다. 부수 효과로 device 0이 선택된다.
	   interrupt도 활성화한다. */
	outb (reg_ctl (c), 0);
	timer_usleep (10);
	outb (reg_ctl (c), CTL_SRST);
	timer_usleep (10);
	outb (reg_ctl (c), 0);

	timer_msleep (150);

	/* device 0이 BSY를 clear할 때까지 기다린다. */
	if (present[0]) {
		select_device (&c->devices[0]);
		wait_while_busy (&c->devices[0]);
	}

	/* device 1이 BSY를 clear할 때까지 기다린다. */
	if (present[1]) {
		int i;

		select_device (&c->devices[1]);
		for (i = 0; i < 3000; i++) {
			if (inb (reg_nsect (c)) == 1 && inb (reg_lbal (c)) == 1)
				break;
			timer_msleep (10);
		}
		wait_while_busy (&c->devices[1]);
	}
}

/* device D가 ATA disk인지 확인하고 D의 is_ata 멤버를 적절히 설정한다.
   D가 device 0(master)이면 이 channel에 slave(device 1)가 존재할 가능성이
   있을 때 true를 반환한다. D가 device 1(slave)이면 반환값은 의미가 없다. */
static bool
check_device_type (struct disk *d) {
	struct channel *c = d->channel;
	uint8_t error, lbam, lbah, status;

	select_device (d);

	error = inb (reg_error (c));
	lbam = inb (reg_lbam (c));
	lbah = inb (reg_lbah (c));
	status = inb (reg_status (c));

	if ((error != 1 && (error != 0x81 || d->dev_no == 1))
			|| (status & STA_DRDY) == 0
			|| (status & STA_BSY) != 0) {
		d->is_ata = false;
		return error != 0x81;
	} else {
		d->is_ata = (lbam == 0 && lbah == 0) || (lbam == 0x3c && lbah == 0xc3);
		return true;
	}
}

/* disk D에 IDENTIFY DEVICE command를 보내고 응답을 읽는다.
   결과를 바탕으로 D의 capacity 멤버를 초기화하고 disk를 설명하는 메시지를
   console에 출력한다. */
static void
identify_ata_device (struct disk *d) {
	struct channel *c = d->channel;
	uint16_t id[DISK_SECTOR_SIZE / 2];

	ASSERT (d->is_ata);

	/* IDENTIFY DEVICE command를 보내고, device 응답이 준비되었음을 나타내는
	   interrupt를 기다린 뒤 data를 buffer로 읽는다. */
	select_device_wait (d);
	issue_pio_command (c, CMD_IDENTIFY_DEVICE);
	sema_down (&c->completion_wait);
	if (!wait_while_busy (d)) {
		d->is_ata = false;
		return;
	}
	input_sector (c, id);

	/* 용량을 계산한다. */
	d->capacity = id[60] | ((uint32_t) id[61] << 16);

	/* 식별 메시지를 출력한다. */
	printf ("%s: detected %'"PRDSNu" sector (", d->name, d->capacity);
	if (d->capacity > 1024 / DISK_SECTOR_SIZE * 1024 * 1024)
		printf ("%"PRDSNu" GB",
				d->capacity / (1024 / DISK_SECTOR_SIZE * 1024 * 1024));
	else if (d->capacity > 1024 / DISK_SECTOR_SIZE * 1024)
		printf ("%"PRDSNu" MB", d->capacity / (1024 / DISK_SECTOR_SIZE * 1024));
	else if (d->capacity > 1024 / DISK_SECTOR_SIZE)
		printf ("%"PRDSNu" kB", d->capacity / (1024 / DISK_SECTOR_SIZE));
	else
		printf ("%"PRDSNu" byte", d->capacity * DISK_SECTOR_SIZE);
	printf (") disk, model \"");
	print_ata_string ((char *) &id[27], 40);
	printf ("\", serial \"");
	print_ata_string ((char *) &id[10], 20);
	printf ("\"\n");
}

/* SIZE byte로 이루어진 STRING을 출력한다. 이 문자열은 각 byte 쌍이 역순인
   특이한 형식이다. 뒤쪽의 공백 및 null은 출력하지 않는다. */
static void
print_ata_string (char *string, size_t size) {
	size_t i;

	/* 마지막 non-white, non-null 문자를 찾는다. */
	for (; size > 0; size--) {
		int c = string[(size - 1) ^ 1];
		if (c != '\0' && !isspace (c))
			break;
	}

	/* 출력한다. */
	for (i = 0; i < size; i++)
		printf ("%c", string[i ^ 1]);
}

/* device D를 선택하고 준비될 때까지 기다린 뒤 SEC_NO를 disk의 sector selection
   register에 쓴다. LBA mode를 사용한다. */
static void
select_sector (struct disk *d, disk_sector_t sec_no) {
	struct channel *c = d->channel;

	ASSERT (sec_no < d->capacity);
	ASSERT (sec_no < (1UL << 28));

	select_device_wait (d);
	outb (reg_nsect (c), 1);
	outb (reg_lbal (c), sec_no);
	outb (reg_lbam (c), sec_no >> 8);
	outb (reg_lbah (c), (sec_no >> 16));
	outb (reg_device (c),
			DEV_MBS | DEV_LBA | (d->dev_no == 1 ? DEV_DEV : 0) | (sec_no >> 24));
}

/* COMMAND를 channel C에 쓰고 completion interrupt를 받을 준비를 한다. */
static void
issue_pio_command (struct channel *c, uint8_t command) {
	/* interrupt가 활성화되어 있어야 completion handler가 semaphore를 up할 수 있다. */
	ASSERT (intr_get_level () == INTR_ON);

	c->expecting_interrupt = true;
	outb (reg_command (c), command);
}

/* PIO mode로 channel C의 data register에서 sector 하나를 SECTOR로 읽는다.
   SECTOR에는 DISK_SECTOR_SIZE byte를 담을 공간이 있어야 한다. */
static void
input_sector (struct channel *c, void *sector) {
	insw (reg_data (c), sector, DISK_SECTOR_SIZE / 2);
}

/* PIO mode로 SECTOR를 channel C의 data register에 쓴다.
   SECTOR는 DISK_SECTOR_SIZE byte를 담고 있어야 한다. */
static void
output_sector (struct channel *c, const void *sector) {
	outsw (reg_data (c), sector, DISK_SECTOR_SIZE / 2);
}

/* low-level ATA primitive. */

/* controller가 idle 상태가 될 때까지 최대 10초 기다린다. 즉 status register의
   BSY와 DRQ bit가 clear될 때까지 기다린다.

   부수 효과로 status register를 읽으면 pending interrupt가 clear된다. */
static void
wait_until_idle (const struct disk *d) {
	int i;

	for (i = 0; i < 1000; i++) {
		if ((inb (reg_status (d->channel)) & (STA_BSY | STA_DRQ)) == 0)
			return;
		timer_usleep (10);
	}

	printf ("%s: idle timeout\n", d->name);
}

/* disk D가 BSY를 clear할 때까지 최대 30초 기다린 뒤 DRQ bit 상태를 반환한다.
   ATA 표준에 따르면 disk reset 완료에 그만큼 오래 걸릴 수 있다. */
static bool
wait_while_busy (const struct disk *d) {
	struct channel *c = d->channel;
	int i;

	for (i = 0; i < 3000; i++) {
		if (i == 700)
			printf ("%s: busy, waiting...", d->name);
		if (!(inb (reg_alt_status (c)) & STA_BSY)) {
			if (i >= 700)
				printf ("ok\n");
			return (inb (reg_alt_status (c)) & STA_DRQ) != 0;
		}
		timer_msleep (10);
	}

	printf ("failed\n");
	return false;
}

/* D가 선택된 disk가 되도록 D의 channel을 설정한다. */
static void
select_device (const struct disk *d) {
	struct channel *c = d->channel;
	uint8_t dev = DEV_MBS;
	if (d->dev_no == 1)
		dev |= DEV_DEV;
	outb (reg_device (c), dev);
	inb (reg_alt_status (c));
	timer_nsleep (400);
}

/* select_device()처럼 D를 channel 안에서 선택하되, 전후로 channel이 idle이
   될 때까지 기다린다. */
static void
select_device_wait (const struct disk *d) {
	wait_until_idle (d);
	select_device (d);
	wait_until_idle (d);
}

/* ATA interrupt handler. */
static void
interrupt_handler (struct intr_frame *f) {
	struct channel *c;

	for (c = channels; c < channels + CHANNEL_CNT; c++)
		if (f->vec_no == c->irq) {
			if (c->expecting_interrupt) {
				inb (reg_status (c));               /* interrupt를 acknowledge한다. */
				sema_up (&c->completion_wait);      /* waiter를 깨운다. */
			} else
				printf ("%s: unexpected interrupt\n", c->name);
			return;
		}

	NOT_REACHED ();
}

static void
inspect_read_cnt (struct intr_frame *f) {
	struct disk * d = disk_get (f->R.rdx, f->R.rcx);
	f->R.rax = d->read_cnt;
}

static void
inspect_write_cnt (struct intr_frame *f) {
	struct disk * d = disk_get (f->R.rdx, f->R.rcx);
	f->R.rax = d->write_cnt;
}

/* disk r/w count를 테스트하기 위한 도구. int 0x43과 int 0x44를 통해 호출한다.
 * 입력:
 *   @RDX - 검사할 disk의 chan_no
 *   @RCX - 검사할 disk의 dev_no
 * 출력:
 *   @RAX - disk의 read/write count. */
void
register_disk_inspect_intr (void) {
	intr_register_int (0x43, 3, INTR_OFF, inspect_read_cnt, "Inspect Disk Read Count");
	intr_register_int (0x44, 3, INTR_OFF, inspect_write_cnt, "Inspect Disk Write Count");
}
