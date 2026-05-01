#include "tests/vm/qsort.h"
#include <stdbool.h>
#include <debug.h>
#include <random.h>

/* BUF의 SIZE 바이트에서 quicksort용 피벗을 고른다. */
static unsigned char
pick_pivot (unsigned char *buf, size_t size) 
{
  ASSERT (size >= 1);
  return buf[random_ulong () % size];
}

/* ARRAY의 SIZE 바이트가 피벗 기준으로 올바르게 나뉘었는지 확인한다. */
static bool
is_partitioned (const unsigned char *array, size_t size,
                unsigned char pivot, size_t left_size) 
{
  size_t i;
  
  for (i = 0; i < left_size; i++)
    if (array[i] >= pivot)
      return false;

  for (; i < size; i++)
    if (array[i] < pivot)
      return false;

  return true;
}

/* *A와 *B의 바이트를 맞바꾼다. */
static void
swap (unsigned char *a, unsigned char *b) 
{
  unsigned char t = *a;
  *a = *b;
  *b = t;
}

/* ARRAY를 제자리에서 분할해 앞쪽에는 피벗보다 작은 바이트들이 오게 한다. */
static size_t
partition (unsigned char *array, size_t size, int pivot) 
{
  size_t left_size = size;
  unsigned char *first = array;
  unsigned char *last = first + left_size;

  for (;;)
    {
      /* FIRST를 앞으로 움직여 피벗보다 큰 첫 원소를 가리키게 한다. */
      for (;;)
        {
          if (first == last)
            {
              ASSERT (is_partitioned (array, size, pivot, left_size));
              return left_size;
            }
          else if (*first >= pivot)
            break;

          first++;
        }
      left_size--;

      /* LAST를 뒤로 움직여 피벗보다 크지 않은 마지막 원소를 가리키게 한다. */
      for (;;)
        {
          last--;

          if (first == last)
            {
              ASSERT (is_partitioned (array, size, pivot, left_size));
              return left_size;
            }
          else if (*last < pivot)
            break;
          else
            left_size--;
        }

      /* FIRST와 LAST를 맞바꾸어 앞쪽/뒤쪽 분할 영역을 확장한다. */
      swap (first, last);
      first++;
    }
}

/* BUF의 SIZE 바이트가 비감소 순서이면 true를 반환한다. */
static bool
is_sorted (const unsigned char *buf, size_t size) 
{
  size_t i;

  for (i = 1; i < size; i++)
    if (buf[i - 1] > buf[i])
      return false;

  return true;
}

/* quicksort를 사용해 BUF의 SIZE 바이트를 비감소 순서로 정렬한다. */
void
qsort_bytes (unsigned char *buf, size_t size) 
{
  if (!is_sorted (buf, size)) 
    {
      int pivot = pick_pivot (buf, size);

      unsigned char *left_half = buf;
      size_t left_size = partition (buf, size, pivot);
      unsigned char *right_half = left_half + left_size;
      size_t right_size = size - left_size;
  
      if (left_size <= right_size) 
        {
          qsort_bytes (left_half, left_size);
          qsort_bytes (right_half, right_size); 
        }
      else
        {
          qsort_bytes (right_half, right_size); 
          qsort_bytes (left_half, left_size);
        }
    } 
}
