#ifndef TESTS_LIB_H
#define TESTS_LIB_H

#include <debug.h>
#include <stdbool.h>
#include <stddef.h>
#include <syscall.h>

extern const char *test_name;
extern bool quiet;

void msg (const char *, ...) PRINTF_FORMAT (1, 2);
void fail (const char *, ...) PRINTF_FORMAT (1, 2) NO_RETURN;

/* SUCCESS를 검사할 표현식과 printf 스타일 인자를 포함할 수 있는 메시지를
   받는다. 메시지를 로그로 남긴 뒤 표현식을 검사한다. 표현식 값이 0이면
   실패를 뜻하므로 그 메시지를 실패 메시지로 출력한다.

   사용할 때 약간 조심해야 한다:

     - SUCCESS에는 메시지에 영향을 주는 부작용이 있으면 안 된다.
       그러면 원래 메시지와 실패 메시지가 달라질 수 있다.

     - 메시지 자체에도 부작용이 있으면 안 된다. 실패 시에는 두 번
       출력되고, quiet가 설정된 성공 시에는 한 번도 출력되지 않기
       때문이다. */
#define CHECK(SUCCESS, ...)                     \
        do                                      \
          {                                     \
            msg (__VA_ARGS__);                  \
            if (!(SUCCESS))                     \
              fail (__VA_ARGS__);               \
          }                                     \
        while (0)

void shuffle (void *, size_t cnt, size_t size);

void exec_children (const char *child_name, pid_t pids[], size_t child_cnt);
void wait_children (pid_t pids[], size_t child_cnt);

void check_file_handle (int fd, const char *file_name,
                        const void *buf_, size_t filesize);
void check_file (const char *file_name, const void *buf, size_t filesize);

void compare_bytes (const void *read_data, const void *expected_data,
                    size_t size, size_t ofs, const char *file_name);

#endif /* test/lib.h */
