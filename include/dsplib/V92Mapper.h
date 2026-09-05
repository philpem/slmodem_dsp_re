/**
 * @file V92Mapper.h
 * @brief The V.92 symbol mapper: bits in, one scaled constellation point out.
 *
 * Four members and one static table, 229 bytes. GCC only emits an
 * out-of-line constructor/destructor for a user-declared one, so the
 * blob's C1/C2/D1/D2 symbols say the original declared both and left the
 * bodies empty -- the object is actually initialised by
 * `reset(short, unsigned char)` instead.
 *
 * The object is 0x2c bytes, measured rather than bounded:
 * `V92Phase4Modulator::V92Phase4Modulator` allocates exactly that many bytes
 * before calling this constructor (finding F1249), agreeing with the
 * furthest member displacement, +0x28, four bytes wide.
 */

#ifndef DSPLIB_V92MAPPER_H
#define DSPLIB_V92MAPPER_H

class V92Mapper {
public:
	/** @brief Construct with no initialization; the object is set up by
	 *         reset() instead. Empty body, as the blob's own out-of-line
	 *         symbol is. */
	V92Mapper();
	/** @brief Destroy. Empty body. */
	~V92Mapper();

	/**
	 * @brief Configure the constellation scale and select which table
	 *        row (and bit count) process() draws from.
	 * @param scale  Multiplier applied to every output point.
	 * @param mode   0 selects a 2-bit constellation (`bits`=2, mean
	 *               power 5.0f); anything else selects 3-bit
	 *               (`bits`=3, mean power 21.0f).
	 */
	void reset(short scale, unsigned char mode);

	/**
	 * @brief Map one symbol's worth of bits to a scaled constellation
	 *        point. Keeps its running accumulator at 16 bits internally,
	 *        matching the object -- invisible at the 2-3 bit widths
	 *        `reset` ever installs (finding F1370).
	 * @param bits  Pointer to `this->bits` input bits (one per byte).
	 * @return `constelAmplitudeTable[bits + 8 * mode]`, divided by
	 *         `sqrt(power)`, multiplied by `scale`, rounded toward zero.
	 */
	int process(unsigned char *bits);

	/** @brief The two constellations' raw amplitude levels, 8 entries
	 *  per row selected by `mode`: row 0 is `{1, 3, -1, -3, 0, 0, 0, 0}`,
	 *  row 1 is `{1, 3, 5, 7, -1, -3, -5, -7}`, agreeing with `reset`
	 *  pairing row 0 with 2 bits and row 1 with 3 (finding F1370). */
	static int constelAmplitudeTable[16];

	/* +0x00  reset()'s scale argument; process() multiplies by it. */
	short scale;

	/* +0x02  reset()'s mode argument, and the table's row selector. */
	unsigned char mode;

	/*
	 * +0x03 .. +0x25  Not referenced by either member that touches the
	 * object.  Thirty-five bytes whose contents are settled by neither
	 * the constructor nor `reset`, and which nothing here names. Checked
	 * and declined for removal as alignment padding -- it is real
	 * unmodelled space, not a gap before an aligned field.
	 */
	unsigned char pad_03[0x23];

	/* +0x26  Bit count: 2 when `mode` is zero, 3 otherwise. */
	short bits;

	/* +0x28  Mean power (5.0f / 21.0f by `mode`); process() takes its
	 * square root as the normalising divisor. */
	float power;
};

#endif /* DSPLIB_V92MAPPER_H */
