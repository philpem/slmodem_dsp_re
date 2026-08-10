/*
 * V90MappingParamsInt.cpp -- the free functions over a V90MappingParams.
 *
 * THE FILE NAME IS INFERRED, NOT MEASURED.  `tools/tumap.py` recovers the
 * original's STT_FILE order and reports `V90MappingParamsInt.cpp` between
 * `V90SpectralShapingFilter.cpp` and `V90Resampler.cpp`; the three functions
 * here sit at 0x33330..0x33562, immediately after V90SpectralShapingFilter's
 * last method (0x33280) and before V90Resampler's first (0x34130).  The
 * extent is a shared bracket rather than an exact one, so this is inference
 * from the ordering and not a fact the object states.
 *
 * Reconstructed from dsplibs.o:
 *
 *   0x33330  286  getConstellationsIndex
 *   0x33450  130  getConstellationMask
 *   0x334e0  130  getCodecConstellationMask
 *
 * All three are UNMANGLED `T` symbols, so the original declared them
 * `extern "C"`; a plain C++ prototype would emit `_Z...` and be a different
 * function.  The range holds NO relocations at all -- `tools/dis.py` prints
 * its "N relocation(s) in this range" banner only when there are some, and
 * for 0x33330..0x33570 it prints nothing -- so these three reference no data
 * symbol and call nothing, not even a debug printf.  Every constant is
 * immediate and every table is reached off the first argument.
 *
 * WHAT `getConstellationsIndex` DOES, and the one thing about it worth
 * pausing over.  It walks constellations 1 to 5 looking for an earlier one
 * that is the same, and the sameness test is:
 *
 *	xor  %esi,%esi			n = 0
 *	cmp  $0x0,%ebx			length
 *	jbe  .Ldone			  ... and if it is zero, skip the loop
 *	  ... compare `length` bytes of both tables, breaking on the first
 *	      difference with `n` left at the index that differed ...
 *   .Ldone:
 *	cmp  %esi,%ebx			n == length ?
 *	jne  .Lnext
 *
 * so a length of zero leaves n = 0, the test compares 0 against 0, and the
 * two constellations are declared identical having compared nothing.  TWO
 * EMPTY CONSTELLATIONS ARE THE SAME CONSTELLATION.  That is the object's
 * behaviour, not an accident of this transcription, and t_v90cmask drives it
 * on purpose rather than avoiding it.
 *
 * Names are invented -- an unmangled symbol carries none -- and describe what
 * the object does with the value.  See V90MappingParams.h for the layout and
 * for what is measured about it.
 */

#include "dsplib/V90MappingParams.h"

/*
 * The distinct constellations, in first-seen order.
 *
 * Constellation 0 is always group 0 and is always its own representative;
 * the object writes both before the loop starts.  The return leaves the
 * group count in %eax and nothing distinguishes signed from unsigned, but the
 * value is a count bounded by 6 and the counter it comes from is compared
 * with `jae` and `jbe` throughout, so it is unsigned here.
 */
extern "C" unsigned int
getConstellationsIndex(V90MappingParams *params, int *group)
{
	unsigned int count = 1;
	unsigned int i;

	params->distinctIndex[0] = 0;
	group[0] = 0;

	for (i = 1; i <= 5; i++) {
		unsigned int j;
		unsigned int length = params->constellationSize[i];

		for (j = 0; j < count; j++) {
			int k = params->distinctIndex[j];
			unsigned int n;

			if (params->constellationSize[k] != length)
				continue;

			/*
			 * `n` survives the loop and is the acceptance test,
			 * which is why the zero-length case reads as a match.
			 */
			for (n = 0; n < length; n++) {
				if (params->constellation[k][n] !=
				    params->constellation[i][n])
					break;
				if (params->codecConstellation[k][n] !=
				    params->codecConstellation[i][n])
					break;
			}
			if (n == length) {
				group[i] = group[k];
				break;
			}
		}

		if (j == count) {
			params->distinctIndex[count] = (int)i;
			group[i] = (int)count;
			count++;
		}
	}

	return count;
}

/*
 * `mask[byte >> 4] |= 1 << (byte & 15)` over one constellation.
 *
 * Only eight entries are cleared, and the object shifts the byte right by
 * four in an 8-bit register (`mov %dl,%al; shr $0x4,%al`) with no further
 * masking, so a table byte of 0x80 or more addresses entry 8 to 15 -- which
 * the function never cleared and the caller may not have sized for.  Both
 * halves of that are the object's and are reproduced.
 *
 * The two functions differ in one expression, the table they read, and the
 * object writes them out twice rather than sharing a helper: the two bodies
 * are byte-identical apart from the `lea` displacement (0x4 against 0x304)
 * and are 130 bytes each.  They are written out twice here for the same
 * reason.
 */
extern "C" void
getConstellationMask(V90MappingParams *params, int which, short *mask)
{
	const unsigned char *table;
	unsigned int length;
	unsigned int n;
	int k, i;

	k = params->distinctIndex[which < 6 ? which : 0];
	length = params->constellationSize[k];
	table = params->constellation[k];

	for (i = 0; i <= 7; i++)
		mask[i] = 0;

	for (n = 0; n < length; n++) {
		unsigned char v = table[n];

		mask[v >> 4] = (short)(mask[v >> 4] | (1 << (v & 15)));
	}
}

extern "C" void
getCodecConstellationMask(V90MappingParams *params, int which, short *mask)
{
	const unsigned char *table;
	unsigned int length;
	unsigned int n;
	int k, i;

	k = params->distinctIndex[which < 6 ? which : 0];
	length = params->constellationSize[k];
	table = params->codecConstellation[k];

	for (i = 0; i <= 7; i++)
		mask[i] = 0;

	for (n = 0; n < length; n++) {
		unsigned char v = table[n];

		mask[v >> 4] = (short)(mask[v >> 4] | (1 << (v & 15)));
	}
}
