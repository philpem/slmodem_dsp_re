/*
 * sysdep.h -- the platform services dsplibs.o imports.
 *
 * `dsplibs.o` calls out to exactly nine functions, all of them thin wrappers
 * over libc that slmodemd supplies in `sysdep_common.c`:
 *
 *     nm -u ../slmodemd/dsplibs.o | grep sysdep
 *
 * slmodemd itself has no header for them -- `sysdep_common.c` defines them
 * and every caller declares what it needs -- so this one is written rather
 * than copied.  It exists because re-declaring an extern in each module is
 * how a signature quietly diverges in one place: `sysdep_memset` takes a
 * `size_t`, and a module that declared it as taking `unsigned` would build
 * fine on a 32-bit target and mis-pass its argument on a 64-bit one.
 *
 * Signatures are those in slmodemd/sysdep_common.c, not guesses.
 *
 * Only what the reconstruction uses is declared here; the string and printf
 * helpers are listed too, because the blob imports them and something in the
 * unreconstructed remainder will need them.
 */

#ifndef DSPLIB_SYSDEP_H
#define DSPLIB_SYSDEP_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);

void *sysdep_memset(void *d, int c, size_t l);
void *sysdep_memcpy(void *d, const void *s, size_t l);
void *sysdep_memchr(const void *s, int c, size_t l);

size_t sysdep_strlen(const char *s);
char *sysdep_strcpy(char *d, const char *s);
char *sysdep_strcat(char *d, const char *s);
int sysdep_strcmp(const char *s1, const char *s2);
char *sysdep_strstr(const char *s1, const char *s2);

int sysdep_vsnprintf(char *str, unsigned size, const char *format, va_list ap);
int sysdep_sprintf(char *buf, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_SYSDEP_H */
