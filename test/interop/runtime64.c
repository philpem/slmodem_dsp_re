/*
 * runtime64.c -- the platform services, for the 64-bit interop build.
 *
 * The differential tests link the blob and must therefore be 32-bit.  The
 * SpanDSP interop tests link a 64-bit system library and must therefore not
 * be.  They are separate binaries for that reason, and this is the second
 * one's runtime: the same sysdep_* services, plus stubs for the modem-core
 * callbacks that b103.c imports but no interop test reaches.
 *
 * Nothing here is shared with the differential harness.  These tests answer a
 * different question -- "is it a correct Bell 103 modem" rather than "is it
 * the same as the blob" -- and keeping the two builds apart keeps that
 * distinction visible in the build system rather than only in prose.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsplib/sysdep.h"

void *sysdep_malloc(unsigned int size) { return malloc(size); }
void sysdep_free(void *mem) { free(mem); }
void *sysdep_memset(void *d, int c, size_t l) { return memset(d, c, l); }
void *sysdep_memcpy(void *d, const void *s, size_t l) { return memcpy(d, s, l); }
void *sysdep_memchr(const void *s, int c, size_t l) { return memchr(s, c, l); }
size_t sysdep_strlen(const char *s) { return strlen(s); }
char *sysdep_strcpy(char *d, const char *s) { return strcpy(d, s); }
char *sysdep_strcat(char *d, const char *s) { return strcat(d, s); }
int sysdep_strcmp(const char *a, const char *b) { return strcmp(a, b); }
char *sysdep_strstr(const char *a, const char *b) { return strstr((char *)a, b); }

int
sysdep_vsnprintf(char *str, unsigned size, const char *fmt, va_list ap)
{
	return vsnprintf(str, size, fmt, ap);
}

int
sysdep_sprintf(char *buf, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsprintf(buf, fmt, ap);
	va_end(ap);
	return n;
}

/* The modem core.  b103.c imports these; no interop test calls them. */
int modem_dp_register(int id, void *op) { (void)id; (void)op; return 0; }
void modem_dp_deregister(int id, void *op) { (void)id; (void)op; }
int modem_get_bits(void *m, int c, unsigned char *b, int n)
{ (void)m; (void)c; (void)b; (void)n; return 0; }
int modem_put_bits(void *m, int c, const unsigned char *b, int n)
{ (void)m; (void)c; (void)b; (void)n; return 0; }
int modem_set_param(void *m, unsigned p, int v)
{ (void)m; (void)p; (void)v; return 0; }
long modem_get_param(void *m, unsigned p) { (void)m; (void)p; return 0; }
