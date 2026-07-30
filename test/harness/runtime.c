/*
 * runtime.c -- the environment dsplibs.o expects, for test binaries.
 *
 * The original object imports 24 symbols.  tools/symmap.py splits them in two
 * and the split is load-bearing:
 *
 *   SHARED    - stateless runtime (sysdep_*, memcpy, __divdi3 ...).  Both the
 *               reconstruction and the reference call the same ones.  Defined
 *               here, unprefixed.
 *
 *   STATEFUL  - callbacks that hand out or consume data (modem_get_bits,
 *               modem_put_bits, the parameter store, the datapump registry).
 *               These are renamed to ref_* so the reference gets its own copy
 *               and cannot race the reconstruction for the same bits.  If the
 *               two shared a bit source, each would eat the bits the other
 *               should have seen and every downstream comparison would be
 *               quietly wrong while still looking like plausible audio.
 *
 * The ref_* shims below abort rather than return a plausible-looking default.
 * A pure-function test should never reach them, so reaching one means the test
 * is exercising more of the blob than intended and its result cannot be
 * trusted.  Tests that legitimately need these (phase 2 onward) will replace
 * them with a driven implementation.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- shared */

void *
sysdep_malloc(unsigned size)
{
	return malloc(size);
}

void
sysdep_free(void *ptr)
{
	free(ptr);
}

void *
sysdep_memcpy(void *dst, const void *src, unsigned n)
{
	return memcpy(dst, src, n);
}

void *
sysdep_memset(void *dst, int c, unsigned n)
{
	return memset(dst, c, n);
}

char *
sysdep_strcpy(char *dst, const char *src)
{
	return strcpy(dst, src);
}

char *
sysdep_strcat(char *dst, const char *src)
{
	return strcat(dst, src);
}

unsigned
sysdep_strlen(const char *s)
{
	return (unsigned)strlen(s);
}

int
sysdep_sprintf(char *buf, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsprintf(buf, fmt, ap);
	va_end(ap);
	return ret;
}

int
sysdep_vsnprintf(char *buf, unsigned size, const char *fmt, va_list ap)
{
	return vsnprintf(buf, size, fmt, ap);
}

/* -------------------------------------------------------- reference side */

unsigned int ref_dsplibs_debug_level = 0;

static void
unexpected(const char *who)
{
	fprintf(stderr,
		"harness: reference called %s(), which this test does not "
		"drive.\nThe test is reaching further into dsplibs.o than "
		"intended; its result is not trustworthy.\n", who);
	abort();
}

int
ref_dsplibs_debug_printf(const char *fmt, ...)
{
	(void)fmt;
	return 0;			/* harmless: logging only */
}

int
ref_modem_debug_log_data(void *m, unsigned id, const void *buf, int len)
{
	(void)m; (void)id; (void)buf; (void)len;
	return 0;			/* harmless: logging only */
}

int ref_modem_get_bits(void *m, int nbits, unsigned char *buf, int n)
{ (void)m; (void)nbits; (void)buf; (void)n; unexpected("modem_get_bits"); return 0; }

int ref_modem_put_bits(void *m, int nbits, unsigned char *buf, int n)
{ (void)m; (void)nbits; (void)buf; (void)n; unexpected("modem_put_bits"); return 0; }

long ref_modem_get_param(void *m, unsigned param)
{ (void)m; (void)param; unexpected("modem_get_param"); return 0; }

long ref_modem_set_param(void *m, unsigned name, int val)
{ (void)m; (void)name; (void)val; unexpected("modem_set_param"); return 0; }

long ref_modem_get_sreg(void *m, unsigned sreg)
{ (void)m; (void)sreg; unexpected("modem_get_sreg"); return 0; }

int ref_modem_send_to_tty(void *m, const void *buf, int n)
{ (void)m; (void)buf; (void)n; unexpected("modem_send_to_tty"); return 0; }

int ref_modem_recv_from_tty(void *m, void *buf, int n)
{ (void)m; (void)buf; (void)n; unexpected("modem_recv_from_tty"); return 0; }

int ref_modem_dp_register(int id, void *op)
{ (void)id; (void)op; return 0; }	/* registration is inert in unit tests */

void ref_modem_dp_deregister(int id, void *op)
{ (void)id; (void)op; }
