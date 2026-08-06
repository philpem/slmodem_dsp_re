/*
 * DiffCoder.h -- the four differential coders, as class templates.
 *
 * Eleven weak symbols in their own `.gnu.linkonce.t.*` sections, so as with
 * DspMath.h and SineWave.h the file name is a description and not a
 * translation unit's (finding 243).
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
	T process(T in);

	T	prev_;		/* +0x00, and the whole object */
};

template <class T>
class SerialDifferentialDecoder {
public:
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
	ParallelDifferentialEncoder(unsigned size);
	~ParallelDifferentialEncoder();

	/*
	 * 0 on success, 1 if `size` exceeds the capacity fixed at construction.
	 * On failure NOTHING is written -- not the state, not `size_`.
	 */
	int reset(unsigned size, T init);

	void process(T *in, T *out);

	T		*state_;	/* +0x00 owned, `capacity_` elements */
	unsigned	 capacity_;	/* +0x04 fixed by the constructor    */
	unsigned	 size_;		/* +0x08 the ACTIVE width, 0 until   */
					/*       the first successful reset  */
};

template <class T>
class ParallelDifferentialDecoder {
public:
	ParallelDifferentialDecoder(unsigned size);
	~ParallelDifferentialDecoder();

	int reset(unsigned size, T init);
	void process(T *in, T *out);

	T		*state_;
	unsigned	 capacity_;
	unsigned	 size_;
};

#endif /* DSPLIB_DIFFCODER_H */
