/* 약 1MB의 임의 데이터를 만든 뒤 여러 청크로 나누고, 각 청크를 정렬한 다음 병합 결과를 검증한다. */

#include "tests/vm/parallel-merge.h"
#include <stdio.h>
#include <syscall.h>
#include "tests/arc4.h"
#include "tests/lib.h"
#include "tests/main.h"

#define CHUNK_SIZE (128 * 1024)
#define CHUNK_CNT 8                             /* 청크 수. */
#define DATA_SIZE (CHUNK_CNT * CHUNK_SIZE)      /* 버퍼 크기. */

unsigned char buf1[DATA_SIZE], buf2[DATA_SIZE];
size_t histogram[256];

/* buf1을 임의 데이터로 초기화한 뒤 각 값의 등장 횟수를 센다. */
static void
init (void)
{
  struct arc4 arc4;
  size_t i;

  msg ("init");

  arc4_init (&arc4, "foobar", 6);
  arc4_crypt (&arc4, buf1, sizeof buf1);
  for (i = 0; i < sizeof buf1; i++)
    histogram[buf1[i]]++;
}

/* SUBPROCESS를 사용해 buf1의 각 청크를 정렬한다. */
static void
sort_chunks (const char *subprocess, int exit_status)
{
  pid_t children[CHUNK_CNT];
  size_t i;

  for (i = 0; i < CHUNK_CNT; i++)
    {
      char fn[128];
      char cmd[128];
      int handle;

      msg ("sort chunk %zu", i);

      /* 이 청크를 파일에 쓴다. */
      snprintf (fn, sizeof fn, "buf%zu", i);
      create (fn, CHUNK_SIZE);
      quiet = true;
      CHECK ((handle = open (fn)) > 1, "open \"%s\"", fn);
      write (handle, buf1 + CHUNK_SIZE * i, CHUNK_SIZE);
      close (handle);

      /* 하위 프로세스로 정렬한다. */
      snprintf (cmd, sizeof cmd, "%s %s", subprocess, fn);
      children[i] = fork (subprocess);
      if (children[i] == 0)
        CHECK ((children[i] = exec (cmd)) != -1, "exec \"%s\"", cmd);
      quiet = false;
    }

  for (i = 0; i < CHUNK_CNT; i++)
    {
      char fn[128];
      int handle;

      CHECK (wait (children[i]) == exit_status, "wait for child %zu", i);

      /* 파일에서 청크를 다시 읽는다. */
      quiet = true;
      snprintf (fn, sizeof fn, "buf%zu", i);
      CHECK ((handle = open (fn)) > 1, "open \"%s\"", fn);
      read (handle, buf1 + CHUNK_SIZE * i, CHUNK_SIZE);
      close (handle);
      quiet = false;
    }
}

/* buf1의 정렬된 청크들을 완전히 정렬된 buf2로 병합한다. */
static void
merge (void)
{
  unsigned char *mp[CHUNK_CNT];
  size_t mp_left;
  unsigned char *op;
  size_t i;

  msg ("merge");

  /* 병합 포인터를 초기화한다. */
  mp_left = CHUNK_CNT;
  for (i = 0; i < CHUNK_CNT; i++)
    mp[i] = buf1 + CHUNK_SIZE * i;

  /* 병합한다. */
  op = buf2;
  while (mp_left > 0)
    {
      /* 가장 작은 값을 찾는다. */
      size_t min = 0;
      for (i = 1; i < mp_left; i++)
        if (*mp[i] < *mp[min])
          min = i;

      /* 값을 buf2에 추가한다. */
      *op++ = *mp[min];

      /* 병합 포인터를 앞으로 이동한다. */
      if ((++mp[min] - buf1) % CHUNK_SIZE == 0)
        mp[min] = mp[--mp_left];
    }
}

static void
verify (void)
{
  size_t buf_idx;
  size_t hist_idx;

  msg ("verify");

  buf_idx = 0;
  for (hist_idx = 0; hist_idx < sizeof histogram / sizeof *histogram;
       hist_idx++)
    {
      while (histogram[hist_idx]-- > 0)
        {
          if (buf2[buf_idx] != hist_idx)
            fail ("bad value %d in byte %zu", buf2[buf_idx], buf_idx);
          buf_idx++;
        }
    }

  msg ("success, buf_idx=%'zu", buf_idx);
}

void
parallel_merge (const char *child_name, int exit_status)
{
  init ();
  sort_chunks (child_name, exit_status);
  merge ();
  verify ();
}
