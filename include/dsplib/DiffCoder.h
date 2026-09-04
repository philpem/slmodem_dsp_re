/*
 * DiffCoder.h -- the four differential coders, as class templates.
 *
 * Eleven weak symbols in their own `.gnu.linkonce.t.*` sections, so as with
 * DspMath.h and SineWave.h the file name is a description and not a
 * translation unit's (finding F243).
 *
 * ---------------------------------------------------------------------------
 * What they are for: V.90 sign-bit differential coding
 *
 * The parallel pair keeps one INDEPENDENT one-symbol memory per position in
 * the six-sample V.90 frame, so each sign bit is coded against the previous
 * frame's bit at the same position rather than against its neighbour.  That is
 * read from the callers, not assumed: `V90SignBitsExtractor` and
 * `V90SpectralShaper` both construct their parallel member with capacity 6 and
 * their `reset` passes `6 / spacing` as the active width.
 *
 * The serial pair is the ordinary one-symbol-back version.
 * `SerialDifferentialDecoder<int>` is called ten times from
 * `V90Phase3Demodulator`; the `unsigned char` encoder from `V90Mapper::process`
 * and the decoder from `V90Demapper::process`.
 *
 * ---------------------------------------------------------------------------
 * THE SERIAL CLASSES HAVE NO CONSTRUCTOR, and that is deliberate
 *
 * The object contains no `SerialDifferential*C1Ev` symbol of any kind, for any
 * of the three instantiations.  Declaring `SerialDifferentialEncoder() : prev_(0)`
 * would be the natural way to write it and would emit one.  With no
 * user-declared constructor the class is trivial, nothing is emitted, and an
 * enclosing class that value-initialises the member gets the initialisation
 * inlined -- which is exactly what `V90SignBitsExtractor`'s constructor does
 * with its `movb $0x0,0x18(%ebx)`.  So the absence is evidence, and it is
 * reproduced by writing no constructor.
 */

#ifndef DSPLIB_DIFFCODER_H
#define DSPLIB_DIFFCODER_H

template <class T>
class SerialDifferentialEncoder {
public:
	/**
	 * @brief XOR-encode one symbol against the last one coded.
	 * @param in  The new symbol.
	 * @return `prev_ ^ in`, which also becomes the new `prev_`.
	 */
	T process(T in);

	T	prev_;		/* +0x00, and the whole object */
};

template <class T>
class SerialDifferentialDecoder {
public:
	/**
	 * @brief Undo SerialDifferentialEncoder::process().
	 * @param in  The coded symbol.
	 * @return `prev_ ^ in`; @p in itself becomes the new `prev_`.
	 */
	T process(T in);

	T	prev_;
};

/*
 * The parallel pair are byte-identical in layout and differ only in `process`.
 * 12 bytes, pinned from outside: `V90SpectralShaper`'s constructor builds one
 * at `%ebx+0x3c` and the next member at `%ebx+0x48`.
 */
template <class T>
class ParallelDifferentialEncoder {
public:
	/**
	 * @brief Allocate a @p size-element state buffer, zeroed.
	 *
	 * Does NOT call reset() and leaves `size_` at 0 -- a freshly
	 * constructed encoder processes nothing at all until reset() is
	 * called (the seemingly-equivalent `{ reset(size, 0); }` is wrong:
	 * it diverges from the object at 43,755 comparison points). The
	 * allocation is not checked before the zero-fill, matching the
	 * object; a NULL return faults on the first store for nonzero @p size.
	 *
	 * @param size  Capacity, fixed for the object's lifetime.
	 */
	ParallelDifferentialEncoder(unsigned size);
	~ParallelDifferentialEncoder();

	/**
	 * @brief Activate a width and fill it with an initial value.
	 *
	 * Fills only the new width: elements at and above @p size keep
	 * whatever they held before (observable when resetting narrower then
	 * wider again -- reproduced as the object's, costs 12,520 comparison
	 * points to "fix").
	 *
	 * @param size  New active width; must not exceed `capacity_`.
	 * @param init  Fill value for the newly active elements.
	 * @return 0 on success, or 1 if @p size exceeds `capacity_` -- on
	 *         failure NOTHING is written, not the state, not `size_`.
	 */
	int reset(unsigned size, T init);

	/**
	 * @brief XOR-encode `size_` symbols, one independent history per
	 * position.
	 * @param in   `size_` input symbols.
	 * @param out  `size_` output symbols: `out[i] = state_[i] ^ in[i]`,
	 *             which also becomes the new `state_[i]`.
	 */
	void process(T *in, T *out);

	T		*state_;	/* +0x00 owned, `capacity_` elements */
	unsigned	 capacity_;	/* +0x04 fixed by the constructor    */
	unsigned	 size_;		/* +0x08 the ACTIVE width, 0 until   */
					/*       the first successful reset  */
};

template <class T>
class ParallelDifferentialDecoder {
public:
	/**
	 * @brief Allocate a @p size-element state buffer, zeroed.
	 *
	 * Same shape as ParallelDifferentialEncoder's constructor, and the
	 * same "does not call reset()" caveat applies. One real asymmetry
	 * from the encoder is reproduced rather than tidied away: the
	 * object's encoder constructor stores 0 into `state_` before the
	 * `sysdep_malloc` call (dead -- overwritten immediately -- but
	 * present) and the decoder's does not.
	 *
	 * @param size  Capacity, fixed for the object's lifetime.
	 */
	ParallelDifferentialDecoder(unsigned size);
	~ParallelDifferentialDecoder();

	/**
	 * @brief Activate a width and fill it with an initial value.
	 * @param size  New active width; must not exceed `capacity_`.
	 * @param init  Fill value for the newly active elements.
	 * @return 0 on success, or 1 if @p size exceeds `capacity_` (see
	 *         ParallelDifferentialEncoder::reset() for the failure/partial
	 *         behavior, which matches).
	 */
	int reset(unsigned size, T init);

	/**
	 * @brief Undo ParallelDifferentialEncoder::process(), one independent
	 * history per position.
	 * @param in   `size_` coded symbols.
	 * @param out  `size_` output symbols: `out[i] = state_[i] ^ in[i]`;
	 *             `in[i]` becomes the new `state_[i]`.
	 */
	void process(T *in, T *out);

	T		*state_;
	unsigned	 capacity_;
	unsigned	 size_;
};

#endif /* DSPLIB_DIFFCODER_H */
