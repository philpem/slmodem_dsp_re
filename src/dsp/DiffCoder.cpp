/*
 * DiffCoder.cpp -- see dsplib/DiffCoder.h.
 *
 * Eleven weak symbols, verified by t_diffcoder against the object over the
 * capacity sweep, every `reset` width either side of the capacity, `process`
 * before any `reset`, in-place operation, both destructor paths and 4,000
 * random symbols per serial instantiation.
 */

#include "dsplib/DiffCoder.h"

extern "C" void *sysdep_malloc(unsigned size);
extern "C" void sysdep_free(void *p);

/*
 * THE REPLACEMENT `operator delete[]`, AND IT IS READ OFF THE OBJECT.  At a
 * destructor's LAST free the blob makes an ordinary `call sysdep_free` where
 * our explicit `if (p) sysdep_free(p)` makes a sibling `jmp` -- one
 * instruction fewer, and the sibcall drops the frame with it.  Eight spellings
 * were compiled and only `delete[]` reproduces the object's shape; finding
 * 7786 and `docs/method/refinement.md` lever 7 carry the enumeration.
 *
 * Behaviourally it is exactly the guard it replaces: the element type is a POD
 * with no destructor, so `delete[] p` is `if (p) operator delete[](p)` and
 * there is no array cookie to read.
 *
 * ONLY THE LAST FREE IN A DESTRUCTOR IS BYTE-EVIDENCE for this.  Away from
 * tail position the two spellings emit identically, so the others carry the
 * same spelling because a destructor written with `delete[]` uses it for every
 * member, not because the object distinguishes them.
 *
 * IT IS A LOCAL COPY AND NOT AN INCLUDE ON PURPOSE.  Hoisting this one
 * definition into `dsplib/sysdep.h` -- which every one of these files already
 * reaches transitively -- moved it earlier in the translation unit and cost
 * EIGHT destructors their byte identity, `FloatFIR` and `FloatARMA` among
 * them.  That is refinement.md lever 3 with an inline function as the carrier,
 * and finding 7815 is the measurement.
 */
inline void operator delete[](void *p) { sysdep_free(p); }

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

/*
 * The instantiations the object contains, and only those.  One template
 * produces both serial-decoder bodies: `prev_ ^ in` is commutative, so the two
 * differ only in the operand order the register allocator chose, and the
 * `movzbl` present on the `unsigned char` return and absent on the `int` one is
 * just "the return type is T".
 */
template class SerialDifferentialEncoder<unsigned char>;
template class SerialDifferentialDecoder<unsigned char>;
template class SerialDifferentialDecoder<int>;
template class ParallelDifferentialEncoder<unsigned char>;
template class ParallelDifferentialDecoder<unsigned char>;
