// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/string.h>

#include <pu32.h>

void pu32printf (const char *fmt, ...) {

	va_list args;

	va_start(args, fmt);
	int vsnprintf (char *buf, size_t size, const char *fmt, va_list args);
	vsnprintf (pu32strbuf, PU32STRBUFSZ, fmt, args);
	va_end(args);

	char *s = pu32strbuf; unsigned n = strlen(pu32strbuf);
	unsigned i; for (i = 0; i < n;)
		i += pu32syswrite (PU32_BIOS_FD_STDOUT, s+i, n-i);
}
