#include <syscall.h>

int
main (int argc, char *argv[])
{
  (void) argc;
  (void) argv;

  write (1, "hello syscall\n", 14);
  return 0;
}
