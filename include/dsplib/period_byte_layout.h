#ifndef DSPLIB_PERIOD_BYTE_LAYOUT_H
#define DSPLIB_PERIOD_BYTE_LAYOUT_H

/*
 * The recovered modem owners overlap physical byte fields and native words.
 * Their partial stores reproduce the little-endian i386 object; they are not
 * portable numeric byte accessors. Do not silently compile that representation
 * on a big-endian target. A port must audit word consumers, byte offsets and
 * caller-visible layouts together, rather than merely reverse union members.
 */
#if defined(__i386__) || defined(__x86_64__)
/* Also works with the period compiler, before __BYTE_ORDER__ existed. */
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
# if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#  error "Recovered modem byte/word layouts require little-endian storage; big-endian port not implemented"
# endif
#else
# include <endian.h>
# if !defined(__BYTE_ORDER) || !defined(__LITTLE_ENDIAN)
#  error "Cannot establish byte order for recovered modem byte/word layouts"
# elif __BYTE_ORDER != __LITTLE_ENDIAN
#  error "Recovered modem byte/word layouts require little-endian storage; big-endian port not implemented"
# endif
#endif

#endif
