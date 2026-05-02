#include <debug.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <syscall.h>

/* source file 이름, 줄 번호, 함수 이름, user-specific 메시지를 출력하고
   user program을 abort한다. */
void
debug_panic (const char *file, int line, const char *function,
		const char *message, ...) {
	va_list args;

	printf ("User process ABORT at %s:%d in %s(): ", file, line, function);

	va_start (args, message);
	vprintf (message, args);
	printf ("\n");
	va_end (args);

	debug_backtrace ();

	exit (1);
}
