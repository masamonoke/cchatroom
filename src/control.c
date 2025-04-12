#include "control.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

void die_impl(const char* func_name, int line, const char* file, const char* msg, ...) {
	int     err;
	va_list args;

	va_start(args, msg);
	err = errno;
	fprintf(stderr, "DIED [%s]: (error code: %d), line %d, file %s\n", func_name, err, line, file);
	vfprintf(stderr, msg, args);
	fprintf(stderr, "\n");
	va_end(args);

	raise(SIGTERM);
}
