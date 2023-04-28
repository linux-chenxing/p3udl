//SPDX-License-Identifier: GPL-3.0-or-later
/*
 *
 */

#include <stdio.h>
#include <stdarg.h>

int log_printf(int level, const char *tag, const char *format,...)
{
	va_list(args);

	printf("%-14s: ", tag);
	va_start(args, format);

	return vprintf(format, args);
}

