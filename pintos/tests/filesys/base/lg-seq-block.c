/* 꽤 큰 파일을 고정 크기 블록 단위로 순차적으로 쓴다. */

#define TEST_SIZE 75678
#define BLOCK_SIZE 513
#include "tests/filesys/base/seq-block.inc"
