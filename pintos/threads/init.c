#include "threads/init.h"
#include <console.h>
#include <debug.h>
#include <limits.h>
#include <random.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "devices/kbd.h"
#include "devices/input.h"
#include "devices/serial.h"
#include "devices/timer.h"
#include "devices/vga.h"
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/thread.h"
#ifdef USERPROG
#include "userprog/process.h"
#include "userprog/exception.h"
#include "userprog/gdt.h"
#include "userprog/syscall.h"
#include "userprog/tss.h"
#endif
#include "tests/threads/tests.h"
#ifdef VM
#include "vm/vm.h"
#endif
#ifdef FILESYS
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "filesys/fsutil.h"
#endif

/* kernel mapping만 가진 page-map-level-4. */
uint64_t *base_pml4;

#ifdef FILESYS
/* -f: file system을 format할 것인가? */
static bool format_filesys;
#endif

/* -q: kernel task 완료 후 power off할 것인가? */
bool power_off_when_done;

bool thread_tests;

static void bss_init (void);
static void paging_init (uint64_t mem_end);

static char **read_command_line (void);
static char **parse_options (char **argv);
static void run_actions (char **argv);
static void usage (void);

static void print_stats (void);


int main (void) NO_RETURN;

/* pintos main program. */
int
main (void) {
	uint64_t mem_end;
	char **argv;

	/* BSS를 지우고 machine의 RAM 크기를 얻는다. */
	bss_init ();

	/* command line을 인자로 나누고 option을 parse한다. */
	argv = read_command_line ();
	argv = parse_options (argv);

	/* lock을 사용할 수 있도록 자신을 thread로 초기화한 뒤 console locking을 활성화한다. */
	thread_init ();
	console_init ();

	/* memory system을 초기화한다. */
	mem_end = palloc_init ();
	malloc_init ();
	paging_init (mem_end);

#ifdef USERPROG
	tss_init ();
	gdt_init ();
#endif

	/* interrupt handler를 초기화한다. */
	intr_init ();
	timer_init ();
	kbd_init ();
	input_init ();
#ifdef USERPROG
	exception_init ();
	syscall_init ();
#endif
	/* thread scheduler를 시작하고 interrupt를 활성화한다. */
	thread_start ();
	serial_init_queue ();
	timer_calibrate ();

#ifdef FILESYS
	/* file system을 초기화한다. */
	disk_init ();
	filesys_init (format_filesys);
#endif

#ifdef VM
	vm_init ();
#endif

	printf ("Boot complete.\n");

	/* kernel command line에 지정된 action을 실행한다. */
	run_actions (argv);

	/* 마무리한다. */
	if (power_off_when_done)
		power_off ();
	thread_exit ();
}

/* BSS를 지운다. */
static void
bss_init (void) {
	/* "BSS"는 0으로 초기화되어야 하는 segment다.
	   실제로 disk에 저장되지도 않고 kernel loader가 0으로 만들지도 않으므로,
	   우리가 직접 0으로 만들어야 한다.

	   BSS segment의 시작과 끝은 linker가 _start_bss와 _end_bss로 기록한다.
	   kernel.lds를 참고한다. */
	extern char _start_bss, _end_bss;
	memset (&_start_bss, 0, &_end_bss - &_start_bss);
}

/* page table을 kernel virtual mapping으로 채운 뒤 CPU가 새 page directory를
 * 사용하도록 설정한다. base_pml4가 생성한 pml4를 가리키게 한다. */
static void
paging_init (uint64_t mem_end) {
	uint64_t *pml4, *pte;
	int perm;
	pml4 = base_pml4 = palloc_get_page (PAL_ASSERT | PAL_ZERO);

	extern char start, _end_kernel_text;
	// physical address [0 ~ mem_end]를
	//   [LOADER_KERN_BASE ~ LOADER_KERN_BASE + mem_end]에 mapping한다.
	for (uint64_t pa = 0; pa < mem_end; pa += PGSIZE) {
		uint64_t va = (uint64_t) ptov(pa);

		perm = PTE_P | PTE_W;
		if ((uint64_t) &start <= va && va < (uint64_t) &_end_kernel_text)
			perm &= ~PTE_W;

		if ((pte = pml4e_walk (pml4, va, 1)) != NULL)
			*pte = pa | perm;
	}

	// cr3를 다시 load한다.
	pml4_activate(0);
}

/* kernel command line을 word로 나누고 argv와 비슷한 배열로 반환한다. */
static char **
read_command_line (void) {
	static char *argv[LOADER_ARGS_LEN / 2 + 1];
	char *p, *end;
	int argc;
	int i;

	argc = *(uint32_t *) ptov (LOADER_ARG_CNT);
	p = ptov (LOADER_ARGS);
	end = p + LOADER_ARGS_LEN;
	for (i = 0; i < argc; i++) {
		if (p >= end)
			PANIC ("command line arguments overflow");

		argv[i] = p;
		p += strnlen (p, end - p) + 1;
	}
	argv[argc] = NULL;

	/* kernel command line을 출력한다. */
	printf ("Kernel command line:");
	for (i = 0; i < argc; i++)
		if (strchr (argv[i], ' ') == NULL)
			printf (" %s", argv[i]);
		else
			printf (" '%s'", argv[i]);
	printf ("\n");

	return argv;
}

/* ARGV[] 안의 option을 parse하고 첫 non-option 인자를 반환한다. */
static char **
parse_options (char **argv) {
	for (; *argv != NULL && **argv == '-'; argv++) {
		char *save_ptr;
		char *name = strtok_r (*argv, "=", &save_ptr);
		char *value = strtok_r (NULL, "", &save_ptr);

		if (!strcmp (name, "-h"))
			usage ();
		else if (!strcmp (name, "-q"))
			power_off_when_done = true;
#ifdef FILESYS
		else if (!strcmp (name, "-f"))
			format_filesys = true;
#endif
		else if (!strcmp (name, "-rs"))
			random_init (atoi (value));
		else if (!strcmp (name, "-mlfqs"))
			thread_mlfqs = true;
#ifdef USERPROG
		else if (!strcmp (name, "-ul"))
			user_page_limit = atoi (value);
		else if (!strcmp (name, "-threads-tests"))
			thread_tests = true;
#endif
		else
			PANIC ("unknown option `%s' (use -h for help)", name);
	}

	return argv;
}

/* ARGV[1]에 지정된 task를 실행한다. */
static void
run_task (char **argv) {
	const char *task = argv[1];

	printf ("Executing '%s':\n", task);
#ifdef USERPROG
	if (thread_tests){
		run_test (task);
	} else {
		process_wait (process_create_initd (task));
	}
#else
	run_test (task);
#endif
	printf ("Execution of '%s' complete.\n", task);
}

/* ARGV[]에 지정된 모든 action을 null pointer sentinel까지 실행한다. */
static void
run_actions (char **argv) {
	/* action 하나. */
	struct action {
		char *name;                       /* action 이름. */
		int argc;                         /* # of args, including action name. */
		void (*function) (char **argv);   /* action을 실행할 함수. */
	};

	/* 지원하는 action table. */
	static const struct action actions[] = {
		{"run", 2, run_task},
#ifdef FILESYS
		{"ls", 1, fsutil_ls},
		{"cat", 2, fsutil_cat},
		{"rm", 2, fsutil_rm},
		{"put", 2, fsutil_put},
		{"get", 2, fsutil_get},
#endif
		{NULL, 0, NULL},
	};

	while (*argv != NULL) {
		const struct action *a;
		int i;

		/* action 이름을 찾는다. */
		for (a = actions; ; a++)
			if (a->name == NULL)
				PANIC ("unknown action `%s' (use -h for help)", *argv);
			else if (!strcmp (*argv, a->name))
				break;

		/* 필요한 인자를 확인한다. */
		for (i = 1; i < a->argc; i++)
			if (argv[i] == NULL)
				PANIC ("action `%s' requires %d argument(s)", *argv, a->argc - 1);

		/* action을 호출하고 앞으로 이동한다. */
		a->function (argv);
		argv += a->argc;
	}

}

/* kernel command line 도움말 메시지를 출력하고 machine을 power off한다. */
static void
usage (void) {
	printf ("\nCommand line syntax: [OPTION...] [ACTION...]\n"
			"Options must precede actions.\n"
			"Actions are executed in the order specified.\n"
			"\nAvailable actions:\n"
#ifdef USERPROG
			"  run 'PROG [ARG...]' Run PROG and wait for it to complete.\n"
#else
			"  run TEST           Run TEST.\n"
#endif
#ifdef FILESYS
			"  ls                 List files in the root directory.\n"
			"  cat FILE           Print FILE to the console.\n"
			"  rm FILE            Delete FILE.\n"
			"Use these actions indirectly via `pintos' -g and -p options:\n"
			"  put FILE           Put FILE into file system from scratch disk.\n"
			"  get FILE           Get FILE from file system into scratch disk.\n"
#endif
			"\nOptions:\n"
			"  -h                 Print this help message and power off.\n"
			"  -q                 Power off VM after actions or on panic.\n"
			"  -f                 Format file system disk during startup.\n"
			"  -rs=SEED           Set random number seed to SEED.\n"
			"  -mlfqs             Use multi-level feedback queue scheduler.\n"
#ifdef USERPROG
			"  -ul=COUNT          Limit user memory to COUNT pages.\n"
#endif
			);
	power_off ();
}


/* bochs 또는 QEMU에서 실행 중이라면 현재 실행 중인 machine을 power down한다. */
void
power_off (void) {
#ifdef FILESYS
	filesys_done ();
#endif

	print_stats ();

	printf ("Powering off...\n");
	outw (0x604, 0x2000);               /* qemu용 poweroff command. */
	for (;;);
}

/* pintos 실행 통계를 출력한다. */
static void
print_stats (void) {
	timer_print_stats ();
	thread_print_stats ();
#ifdef FILESYS
	disk_print_stats ();
#endif
	console_print_stats ();
	kbd_print_stats ();
#ifdef USERPROG
	exception_print_stats ();
#endif
}
