/* lib/stdlib.c의 정렬과 검색 테스트 프로그램.

   Pintos의 다른 곳에서 충분히 테스트되지 않는 정렬과 검색 기능을
   테스트하려고 시도한다.

   이 테스트는 제출한 프로젝트에서 실행할 테스트는 아니다.
   완결성을 위해 여기에 들어 있다.
*/

#undef NDEBUG
#include <debug.h>
#include <limits.h>
#include <random.h>
#include <stdlib.h>
#include <stdio.h>
#include "threads/test.h"

/* 테스트할 배열의 최대 원소 수. */
#define MAX_CNT 4096

static void shuffle (int[], size_t);
static int compare_ints (const void *, const void *);
static void verify_order (const int[], size_t);
static void verify_bsearch (const int[], size_t);

/* 정렬과 검색 구현을 테스트한다. */
void
test (void) 
{
  int cnt;

  printf ("testing various size arrays:");
  for (cnt = 0; cnt < MAX_CNT; cnt = cnt * 4 / 3 + 1)
    {
      int repeat;

      printf (" %zu", cnt);
      for (repeat = 0; repeat < 10; repeat++) 
        {
          static int values[MAX_CNT];
          int i;

          /* 0...CNT 값을 임의 순서로 VALUES에 넣는다. */
          for (i = 0; i < cnt; i++)
            values[i] = i;
          shuffle (values, cnt);
  
          /* VALUES를 정렬한 뒤 순서를 검증한다. */
          qsort (values, cnt, sizeof *values, compare_ints);
          verify_order (values, cnt);
          verify_bsearch (values, cnt);
        }
    }
  
  printf (" done\n");
  printf ("stdlib: PASS\n");
}

/* ARRAY의 CNT개 원소를 임의 순서로 섞는다. */
static void
shuffle (int *array, size_t cnt) 
{
  size_t i;

  for (i = 0; i < cnt; i++)
    {
      size_t j = i + random_ulong () % (cnt - i);
      int t = array[j];
      array[j] = array[i];
      array[i] = t;
    }
}

/* *A가 *B보다 크면 1, 같으면 0, 작으면 -1을 반환한다. */
static int
compare_ints (const void *a_, const void *b_) 
{
  const int *a = a_;
  const int *b = b_;

  return *a < *b ? -1 : *a > *b;
}

/* ARRAY가 CNT개의 정수 0...CNT-1을 포함하는지 검증한다. */
static void
verify_order (const int *array, size_t cnt) 
{
  int i;

  for (i = 0; (size_t) i < cnt; i++) 
    ASSERT (array[i] == i);
}

/* ARRAY에서 bsearch()가 제대로 동작하는지 확인한다. ARRAY는 0...CNT-1
   값을 포함해야 한다. */
static void
verify_bsearch (const int *array, size_t cnt) 
{
  int not_in_array[] = {0, -1, INT_MAX, MAX_CNT, MAX_CNT + 1, MAX_CNT * 2};
  int i;

  /* 배열 안의 모든 값을 제대로 찾는지 확인한다. */
  for (i = 0; (size_t) i < cnt; i++) 
    ASSERT (bsearch (&i, array, cnt, sizeof *array, compare_ints)
            == array + i);

  /* 배열에 없는 일부 값들이 발견되지 않는지 확인한다. */
  not_in_array[0] = cnt;
  for (i = 0; (size_t) i < sizeof not_in_array / sizeof *not_in_array; i++) 
    ASSERT (bsearch (&not_in_array[i], array, cnt, sizeof *array, compare_ints)
            == NULL);
}
