/*
 * v32sre_tables.c -- V.32 symbol-timing recovery: the six data tables.
 *
 * Extracted from dsplibs.o with tools/tabdump.py and compared byte for byte
 * against the blob's own copies by t_fpm_sre.
 *
 * SREv32_CFG, which points at all six, is NOT here: it is 14 dwords whose
 * field boundaries cannot be settled until FPM_SRE_recover is read, and a
 * byte comparison would pass for every wrong reading of them alike.  See
 * finding 1615.
 *
 * WIDTHS.  `SREv32_COFFS` is proven `short`: FPM_SRE_init copies it with
 * `movzwl (%ecx,%edx,2)`, and its 0x16a bytes are therefore 181 entries.  The
 * other five are read only by FPM_SRE_recover, which is not yet reconstructed,
 * so THEIR ELEMENT WIDTH IS UNVERIFIED -- 16 bits is the shape the block's
 * other tables use and the shape their contents suggest, not a measurement.
 *
 * CONST-NESS follows the sections: SREv32_COFFS is in .rodata, the other five
 * are in .data and are therefore not const.
 *
 * xCLOCK and yCLOCK are cos and sin of 0, 120 and 240 degrees at a scale of
 * 16384 -- the three-phase clock phasor of a 2400-baud signal sampled at
 * 7200 Hz, the same three-samples-a-symbol structure the echo canceller's
 * three coefficient sets carry (finding 1612).
 */

#include "dsplib/fpm_sre.h"

/* SREv32_COFFS[181] - extracted from /home/philpem/dev/sip-D-modem/slmodemd/dsplibs.o .rodata:0x006ea0 by tools/tabdump.py.
 * Reference bytes only: the maintainable form is a generator that
 * reproduces these values from the table's design parameters. */
const short SREv32_COFFS[181] = {
	45, 62, 75, 82, 81, 72, 52, 24, -10, -50,
	-89, -126, -153, -169, -169, -152, -116, -64, 0, 72,
	146, 213, 266, 298, 303, 278, 223, 139, 33, -86,
	-210, -325, -419, -480, -499, -470, -392, -267, -105, 82,
	279, 467, 625, 735, 783, 758, 656, 480, 241, -42,
	-347, -645, -907, -1102, -1207, -1202, -1079, -838, -492, -65,
	408, 888, 1327, 1679, 1902, 1959, 1829, 1503, 992, 324,
	-456, -1288, -2099, -2808, -3335, -3604, -3551, -3128, -2310, -1096,
	487, 2388, 4528, 6812, 9126, 11352, 13371, 15074, 16364, 17168,
	17442, 17168, 16364, 15074, 13371, 11352, 9126, 6812, 4528, 2388,
	487, -1096, -2310, -3128, -3551, -3604, -3335, -2808, -2099, -1288,
	-456, 324, 992, 1503, 1829, 1959, 1902, 1679, 1327, 888,
	408, -65, -492, -838, -1079, -1202, -1207, -1102, -907, -645,
	-347, -42, 241, 480, 656, 758, 783, 735, 625, 467,
	279, 82, -105, -267, -392, -470, -499, -480, -419, -325,
	-210, -86, 33, 139, 223, 278, 303, 298, 266, 213,
	146, 72, 0, -64, -116, -152, -169, -169, -153, -126,
	-89, -50, -10, 24, 52, 72, 81, 82, 75, 62,
	45,
};

/* SREv32_XB_COFFS[11] - extracted from /home/philpem/dev/sip-D-modem/slmodemd/dsplibs.o .data:0x00760c by tools/tabdump.py.
 * Reference bytes only: the maintainable form is a generator that
 * reproduces these values from the table's design parameters. */
short SREv32_XB_COFFS[11] = {
	-28156, 16128, 28156, 16128, 14078, 8128, -14078, 8128, 992, -15360,
	14399,
};

/* SREv32_PLL_K1[3] - extracted from /home/philpem/dev/sip-D-modem/slmodemd/dsplibs.o .data:0x007606 by tools/tabdump.py.
 * Reference bytes only: the maintainable form is a generator that
 * reproduces these values from the table's design parameters. */
short SREv32_PLL_K1[3] = {
	2336, 3049, 1524,
};

/* SREv32_PLL_K2[3] - extracted from /home/philpem/dev/sip-D-modem/slmodemd/dsplibs.o .data:0x007600 by tools/tabdump.py.
 * Reference bytes only: the maintainable form is a generator that
 * reproduces these values from the table's design parameters. */
short SREv32_PLL_K2[3] = {
	0, 17, 8,
};

/* SREv32_xCLOCK[3] - extracted from /home/philpem/dev/sip-D-modem/slmodemd/dsplibs.o .data:0x007628 by tools/tabdump.py.
 * Reference bytes only: the maintainable form is a generator that
 * reproduces these values from the table's design parameters. */
short SREv32_xCLOCK[3] = {
	16384, -8192, -8192,
};

/* SREv32_yCLOCK[3] - extracted from /home/philpem/dev/sip-D-modem/slmodemd/dsplibs.o .data:0x007622 by tools/tabdump.py.
 * Reference bytes only: the maintainable form is a generator that
 * reproduces these values from the table's design parameters. */
short SREv32_yCLOCK[3] = {
	0, 14189, -14189,
};
