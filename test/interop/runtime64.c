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

/*
 * Filled to the same pattern as the differential harness, for the same
 * reason: a field the constructor leaves alone should be obviously wrong
 * rather than plausibly zero.  The interop tier builds real objects through
 * V8Create and B103FP_create, so it is exposed to exactly the same
 * uninitialised reads.
 */
#define INTEROP_MALLOC_FILL	0xa5

void *
sysdep_malloc(unsigned int size)
{
	void *p = malloc(size);

	if (p != NULL)
		memset(p, INTEROP_MALLOC_FILL, size);
	return p;
}

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
long modem_set_param(void *m, unsigned p, int v)
{ (void)m; (void)p; (void)v; return 0; }
long modem_get_param(void *m, unsigned p) { (void)m; (void)p; return 0; }
long modem_get_sreg(void *m, unsigned n) { (void)m; (void)n; return 0; }

/*
 * The FDSP kernel's not-yet-reconstructed callees.  fdspkrnl.c imports
 * them (the differential build forwards them to the blob's own code); no
 * interop test enters the voice path, so reaching one here is a test bug
 * and aborts rather than returning something plausible.
 */
void FDSP_Kernel_InitObj(void *k) { (void)k; abort(); }

/*
 * The diagnostic hooks.
 *
 * Duplicated from test/harness/runtime.c rather than shared, because the two
 * runtimes are deliberately separate builds: this one is 64-bit and links no
 * reference object, so it has no `ref_` half and cannot include the other.
 *
 * Zero, which is slmodemd's default and what the differential tier uses, so
 * the interop build takes the same branch through every gated call site.
 *
 * Their absence here broke `make interop` the moment v34filters.c became the
 * first module to import them, and `make test` did not notice -- the two
 * tiers link different runtimes.  Adding a module to $(SRC) therefore means
 * running BOTH targets, which is now what the phase-boundary check does.
 */
unsigned int dsplibs_debug_level = 0;

int dsplibs_debug_printf(const char *fmt, ...) { (void)fmt; return 0; }

/*
 * The 64-bit build has no blob to compare against, so capture is a stub --
 * but the symbols must exist, because harness.h declares them and the two
 * tiers link different runtimes.  Forgetting that broke `make interop` for
 * two commits once already.
 */
int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void) { }
const char *dsplib_debug_capture_text(int side) { (void)side; return ""; }
unsigned dsplib_debug_capture_lines(int side) { (void)side; return 0; }

int modem_debug_log_data(void *m, unsigned id, const void *b, int l)
{ (void)m; (void)id; (void)b; (void)l; return 0; }
