/* 자식이 fork에 실패할 때까지 재귀적으로 fork한다. */

#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <syscall.h>
#include <random.h>
#include "tests/lib.h"

static const int EXPECTED_DEPTH_TO_PASS = 10;
static const int EXPECTED_REPETITIONS = 10;

int make_children (void);

/* 여러 파일을 열고 닫지 않는다. */
static void
consume_some_resources (void)
{
  int fd, fdmax = 126;

  /* fdmax까지 가능한 한 많은 파일을 연다. */
  for (fd = 0; fd < fdmax; fd++) {
#ifdef EXTRA2
	  if (fd != 0 && (random_ulong () & 1)) {
		if (dup2(random_ulong () % fd, fd+fdmax) == -1)
			break;
		else
			if (open (test_name) == -1)
			  break;
	  }
#else
		if (open (test_name) == -1)
		  break;
#endif
  }
}

/* 일부 리소스를 소비한 뒤 이 프로세스를 종료한다. */
static int NO_INLINE
consume_some_resources_and_die (void)
{
  consume_some_resources ();
  int *KERN_BASE = (int *)0x8004000000;

  switch (random_ulong () % 5) {
	case 0:
	  *(int *) NULL = 42;
    break;

	case 1:
	  return *(int *) NULL;

	case 2:
	  return *KERN_BASE;

	case 3:
	  *KERN_BASE = 42;
    break;

	case 4:
	  open ((char *)KERN_BASE);
	  exit (-1);
    break;

	default:
	  NOT_REACHED ();
  }
  return 0;
}

int
make_children (void) {
  int i = 0;
  int pid;
  char child_name[128];
  for (; ; random_init (i), i++) {
    if (i > EXPECTED_DEPTH_TO_PASS/2) {
      snprintf (child_name, sizeof child_name, "%s_%d_%s", "child", i, "X");
      pid = fork(child_name);
      if (pid > 0 && wait (pid) != -1) {
        fail ("crashed child should return -1.");
      } else if (pid == 0) {
        consume_some_resources_and_die();
        fail ("Unreachable");
      }
    }

    snprintf (child_name, sizeof child_name, "%s_%d_%s", "child", i, "O");
    pid = fork(child_name);
    if (pid < 0) {
      exit (i);
    } else if (pid == 0) {
      consume_some_resources();
    } else {
      break;
    }
  }

  int depth = wait (pid);
  if (depth < 0)
	  fail ("Should return > 0.");

  if (i == 0)
	  return depth;
  else
	  exit (depth);
}

int
main (int argc UNUSED, char *argv[] UNUSED) {
  test_name = "multi-oom";

  msg ("begin");

  int first_run_depth = make_children ();
  CHECK (first_run_depth >= EXPECTED_DEPTH_TO_PASS, "Spawned at least %d children.", EXPECTED_DEPTH_TO_PASS);

  for (int i = 0; i < EXPECTED_REPETITIONS; i++) {
    int current_run_depth = make_children();
    if (current_run_depth < first_run_depth) {
      fail ("should have forked at least %d times, but %d times forked", 
              first_run_depth, current_run_depth);
    }
  }

  msg ("success. Program forked %d iterations.", EXPECTED_REPETITIONS);
  msg ("end");
}
