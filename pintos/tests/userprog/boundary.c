/* 페이지 경계 근처 포인터로 시스템 콜을 깨뜨리려는 테스트용 유틸리티 함수. */

#include <inttypes.h>
#include <round.h>
#include <string.h>
#include "tests/userprog/boundary.h"

static char dst[8192];

/* 페이지의 시작 주소를 반환한다. 이 페이지에는 최소 2048바이트의 여유가 있다. */
void *
get_boundary_area (void) 
{
  char *p = (char *) ROUND_UP ((uintptr_t) dst, 4096);
  if (p - dst < 2048)
    p += 4096;
  return p;
}

/* SRC의 복사본을 두 페이지의 경계에 걸치도록 만들어 반환한다. */
char *
copy_string_across_boundary (const char *src) 
{
  char *p = get_boundary_area ();
  p -= strlen (src) < 4096 ? strlen (src) / 2 : 4096;
  strlcpy (p, src, 4096);
  return p;
}

