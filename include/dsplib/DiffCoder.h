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

#include "dsplib/sysdep.h"

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

template <class T>
T SerialDifferentialEncoder<T>::process(T in)
{
	prev_ = prev_ ^ in;
	return prev_;
}

template <class T>
T SerialDifferentialDecoder<T>::process(T in)
{
	T out = prev_ ^ in;

	prev_ = in;
	return out;
}

/*
 * THE CONSTRUCTOR DOES NOT CALL `reset`, and `size_` is left at 0.  A freshly
 * constructed coder therefore processes NOTHING -- `process` runs zero
 * iterations and writes no output at all until the owner has called `reset`.
 * Writing the constructor as `{ reset(size, 0); }` is the obvious tidy form and
 * is wrong by 43,755 comparison points.
 *
 * The allocation is not checked before the zero-fill, so a NULL return faults
 * on the first store for any non-zero `size`.  The object does not check
 * either.
 */
template <class T>
ParallelDifferentialEncoder<T>::ParallelDifferentialEncoder(unsigned size)
	: state_(0), capacity_(size), size_(0)
{
	unsigned i;

	state_ = (T *)sysdep_malloc(size);
	for (i = 0; i < size; i++)
		state_[i] = 0;
}

template <class T>
ParallelDifferentialEncoder<T>::~ParallelDifferentialEncoder()
{
	/* `state_` is NOT nulled, so a second destruction double-frees. */
	delete[] state_;
}

/*
 * `reset` fills only the NEW width, not the capacity: elements at and above
 * `size` keep whatever they held from before.  That is observable whenever a
 * caller resets to a narrower width and then back to a wider one, and it costs
 * 12,520 comparison points to "fix".
 */
template <class T>
int ParallelDifferentialEncoder<T>::reset(unsigned size, T init)
{
	unsigned i;

	if (capacity_ < size)
		return 1;
	for (i = 0; i < size; i++)
		state_[i] = init;
	size_ = size;
	return 0;
}

template <class T>
void ParallelDifferentialEncoder<T>::process(T *in, T *out)
{
	unsigned i;

	for (i = 0; i < size_; i++) {
		state_[i] = state_[i] ^ in[i];
		out[i] = state_[i];
	}
}

/*
 * The decoder's constructor is the encoder's WITHOUT the `state_(0)`.
 *
 * The object really does differ here: the encoder emits `movl $0x0,(%ebx)`
 * before calling `sysdep_malloc` and the decoder emits nothing.  The store is
 * dead -- the malloc result overwrites it immediately -- so no test can see
 * it, but it is a real asymmetry between two otherwise identical classes in
 * the original's source and it is reproduced rather than tidied into
 * agreement.  Adding or removing the initialiser makes the store appear or
 * vanish in our build too, which is how it was confirmed.
 */
template <class T>
ParallelDifferentialDecoder<T>::ParallelDifferentialDecoder(unsigned size)
	: capacity_(size), size_(0)
{
	unsigned i;

	state_ = (T *)sysdep_malloc(size);
	for (i = 0; i < size; i++)
		state_[i] = 0;
}

template <class T>
ParallelDifferentialDecoder<T>::~ParallelDifferentialDecoder()
{
	delete[] state_;
}

template <class T>
int ParallelDifferentialDecoder<T>::reset(unsigned size, T init)
{
	unsigned i;

	if (capacity_ < size)
		return 1;
	for (i = 0; i < size; i++)
		state_[i] = init;
	size_ = size;
	return 0;
}

template <class T>
void ParallelDifferentialDecoder<T>::process(T *in, T *out)
{
	unsigned i;

	for (i = 0; i < size_; i++) {
		T x = in[i];

		out[i] = x ^ state_[i];
		state_[i] = x;
	}
}

#endif /* DSPLIB_DIFFCODER_H */
