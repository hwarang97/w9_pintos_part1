/* /0/0/0부터 /3/2/2까지 디렉터리를 만들고 그 안에 파일을 만든다. */

#include "tests/filesys/extended/mk-tree.h"
#include "tests/main.h"

void
test_main (void) 
{
  make_tree (4, 3, 3, 4);
}

