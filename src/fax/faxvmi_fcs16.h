#ifndef DSPLIB_FAXVMI_FCS16_H
#define DSPLIB_FAXVMI_FCS16_H

/* The native public CRC routine and frame writer contain this same two-nibble
 * walk. Share its implementation while keeping the public entry point in its
 * original translation unit. Visibility of the original inline is not unique.
 * Both count consumption and the narrowing before each final OR are explicit.
 */
#define FCS16_NIBBLE(fcs, shifted)					\
	do {								\
		unsigned int t_ = ((shifted) ^ (fcs)) & 0xf000u;	\
		(fcs) = (unsigned short)					\
			((((fcs) ^ (t_ >> 11)) << 4) ^ t_); \
		(fcs) |= t_ >> 12; \
	} while (0)

static inline int
faxvmi_fcs16_inline(unsigned short *buf, short count)
{
	unsigned short fcs = 0xffff;
	short i;

	for (i = count; i-- != 0;) {
		unsigned int octet = *buf++;

		FCS16_NIBBLE(fcs, octet << 8);
		FCS16_NIBBLE(fcs, octet << 12);
	}
	return (unsigned short)~fcs;
}

#undef FCS16_NIBBLE

#endif
