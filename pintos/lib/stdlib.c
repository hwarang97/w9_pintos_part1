#include <ctype.h>
#include <debug.h>
#include <random.h>
#include <stdlib.h>
#include <stdbool.h>

/* S 안의 부호 있는 10진 정수 문자열 표현을 int로 변환해 반환한다. */
int
atoi (const char *s) 
{
  bool negative;
  int value;

  ASSERT (s != NULL);

  /* 공백을 건너뛴다. */
  while (isspace ((unsigned char) *s))
    s++;

  /* 부호를 해석한다. */
  negative = false;
  if (*s == '+')
    s++;
  else if (*s == '-')
    {
      negative = true;
      s++;
    }

  /* 숫자를 해석한다.  2의 보수 시스템에서는 int의 음수 범위가
     양수 범위보다 크기 때문에, 처음에는 항상 음수 값으로 해석한 뒤
     나중에 양수로 바꾼다. */
  for (value = 0; isdigit (*s); s++)
    value = value * 10 - (*s - '0');
  if (!negative)
    value = -value;

  return value;
}

/* AUX 함수를 호출해 A와 B를 비교한다. */
static int
compare_thunk (const void *a, const void *b, void *aux) 
{
  int (**compare) (const void *, const void *) = aux;
  return (*compare) (a, b);
}

/* SIZE 바이트짜리 원소 CNT개를 담은 ARRAY를 COMPARE로 정렬한다.
   COMPARE가 원소 A와 B를 받으면 strcmp()와 같은 형식의 결과를
   반환해야 한다. 즉 A < B이면 0보다 작은 값, A == B이면 0,
   A > B이면 0보다 큰 값을 반환한다.  CNT에 대해 O(n lg n) 시간과
   O(1) 공간으로 실행된다. */
void
qsort (void *array, size_t cnt, size_t size,
       int (*compare) (const void *, const void *)) 
{
  sort (array, cnt, size, compare_thunk, &compare);
}

/* SIZE 바이트짜리 원소를 담은 ARRAY에서 1부터 시작하는 인덱스
   A_IDX와 B_IDX의 원소를 맞바꾼다. */
static void
do_swap (unsigned char *array, size_t a_idx, size_t b_idx, size_t size)
{
  unsigned char *a = array + (a_idx - 1) * size;
  unsigned char *b = array + (b_idx - 1) * size;
  size_t i;

  for (i = 0; i < size; i++)
    {
      unsigned char t = a[i];
      a[i] = b[i];
      b[i] = t;
    }
}

/* SIZE 바이트짜리 원소를 담은 ARRAY에서 1부터 시작하는 인덱스
   A_IDX와 B_IDX의 원소를 비교한다.  원소 비교에는 COMPARE를
   사용하고 AUX를 보조 데이터로 넘기며, strcmp() 형식의 결과를
   반환한다. */
static int
do_compare (unsigned char *array, size_t a_idx, size_t b_idx, size_t size,
            int (*compare) (const void *, const void *, void *aux),
            void *aux) 
{
  return compare (array + (a_idx - 1) * size, array + (b_idx - 1) * size, aux);
}

/* SIZE 바이트짜리 원소 CNT개를 담은 ARRAY에서 1부터 시작하는
   인덱스 I의 원소를 힙 아래쪽으로 내려 보낸다.  원소 비교에는
   COMPARE를 사용하고 AUX를 보조 데이터로 넘긴다. */
static void
heapify (unsigned char *array, size_t i, size_t cnt, size_t size,
         int (*compare) (const void *, const void *, void *aux),
         void *aux) 
{
  for (;;) 
    {
      /* `max'를 I와 그 자식들(있다면) 중 가장 큰 원소의 인덱스로
         설정한다. */
      size_t left = 2 * i;
      size_t right = 2 * i + 1;
      size_t max = i;
      if (left <= cnt && do_compare (array, left, max, size, compare, aux) > 0)
        max = left;
      if (right <= cnt
          && do_compare (array, right, max, size, compare, aux) > 0) 
        max = right;

      /* 최댓값이 이미 원소 I에 있으면 작업이 끝난 것이다. */
      if (max == i)
        break;

      /* 맞바꾸고 힙 아래쪽으로 계속 내려간다. */
      do_swap (array, i, max, size);
      i = max;
    }
}

/* SIZE 바이트짜리 원소 CNT개를 담은 ARRAY를 정렬한다.
   원소 비교에는 COMPARE를 사용하고 AUX를 보조 데이터로 넘긴다.
   COMPARE가 원소 A와 B를 받으면 strcmp()와 같은 형식의 결과를
   반환해야 한다. 즉 A < B이면 0보다 작은 값, A == B이면 0,
   A > B이면 0보다 큰 값을 반환한다.  CNT에 대해 O(n lg n) 시간과
   O(1) 공간으로 실행된다. */
void
sort (void *array, size_t cnt, size_t size,
      int (*compare) (const void *, const void *, void *aux),
      void *aux) 
{
  size_t i;

  ASSERT (array != NULL || cnt == 0);
  ASSERT (compare != NULL);
  ASSERT (size > 0);

  /* 힙을 만든다. */
  for (i = cnt / 2; i > 0; i--)
    heapify (array, i, cnt, size, compare, aux);

  /* 힙을 정렬한다. */
  for (i = cnt; i > 1; i--) 
    {
      do_swap (array, 1, i, size);
      heapify (array, 1, i - 1, size, compare, aux); 
    }
}

/* SIZE 바이트짜리 원소 CNT개를 담은 ARRAY에서 주어진 KEY를 찾는다.
   일치하는 원소를 찾으면 그 원소를 반환하고, 없으면 널 포인터를
   반환한다.  일치하는 원소가 여러 개면 그중 임의의 하나를 반환한다.

   ARRAY는 COMPARE 기준으로 정렬되어 있어야 한다.

   원소 비교에는 COMPARE를 사용한다.  COMPARE가 원소 A와 B를 받으면
   strcmp()와 같은 형식의 결과를 반환해야 한다. 즉 A < B이면 0보다
   작은 값, A == B이면 0, A > B이면 0보다 큰 값을 반환한다. */
void *
bsearch (const void *key, const void *array, size_t cnt,
         size_t size, int (*compare) (const void *, const void *)) 
{
  return binary_search (key, array, cnt, size, compare_thunk, &compare);
}

/* SIZE 바이트짜리 원소 CNT개를 담은 ARRAY에서 주어진 KEY를 찾는다.
   일치하는 원소를 찾으면 그 원소를 반환하고, 없으면 널 포인터를
   반환한다.  일치하는 원소가 여러 개면 그중 임의의 하나를 반환한다.

   ARRAY는 COMPARE 기준으로 정렬되어 있어야 한다.

   원소 비교에는 COMPARE를 사용하고 AUX를 보조 데이터로 넘긴다.
   COMPARE가 원소 A와 B를 받으면 strcmp()와 같은 형식의 결과를
   반환해야 한다. 즉 A < B이면 0보다 작은 값, A == B이면 0,
   A > B이면 0보다 큰 값을 반환한다. */
void *
binary_search (const void *key, const void *array, size_t cnt, size_t size,
               int (*compare) (const void *, const void *, void *aux),
               void *aux) 
{
  const unsigned char *first = array;
  const unsigned char *last = array + size * cnt;

  while (first < last) 
    {
      size_t range = (last - first) / size;
      const unsigned char *middle = first + (range / 2) * size;
      int cmp = compare (key, middle, aux);

      if (cmp < 0) 
        last = middle;
      else if (cmp > 0) 
        first = middle + size;
      else
        return (void *) middle;
    }
  
  return NULL;
}

