#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include "error.h"

void pr_error(int output_strerror, char *fmt, ...)
{
	va_list args;
	char buf[1024];

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	printf("%% %s", buf);
	if (output_strerror)
		printf(" (%s)", strerror(errno));
	printf("\n");
}

