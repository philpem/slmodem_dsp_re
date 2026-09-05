/*
 * DspMath.h -- the float statistics and window functions, as templates.
 *
 * Eleven function templates the original instantiates at `float` and nothing
 * else.  They are emitted weak, one per section, so they carry no `.text`
 * address and the translation-unit map cannot place them -- which is why this
 * file's name is a description rather than the original's, and why
 * docs/modules.md has no entry to match.  See finding F243.
 *
 * The signatures are exact and were not guessed: a FUNCTION TEMPLATE encodes
 * its return type in the mangling, unlike an ordinary function, so
 * `_Z3StdIfET_PS0_j` says `float Std<float>(float*, unsigned)` outright.
 * Finding F226 has the general point; this is the case where it pays twice.
 *
 * ---------------------------------------------------------------------------
 * The statistics are a chain, and `sqrSum` is misnamed
 *
 *     sum     Sx
 *     mean    Sx / n
 *     sqrSum  Sx^2 / n     <-- a MEAN of squares, despite the name
 *     Var     sqrSum - mean^2
 *     Std     sqrt(Var)
 *
 * `sqrSum` divides by `n` before returning, so `Var` is the textbook
 * E[x^2] - E[x]^2 and not a sum of anything.  The name is the original's and
 * is kept; the behaviour is what the object does.
 *
 * Each is built on the one above it -- `Var` really does call `sqrSum` and
 * `mean` rather than walking the array once -- so the rounding is the
 * original's chain of roundings and not a tidier equivalent.
 */

#ifndef DSPLIB_DSPMATH_H
#define DSPLIB_DSPMATH_H

/*
 * The window shapes `designWindow` selects between.  The names are the
 * author's, recovered from the mangling of `designWindow<float>` and of
 * `LowPassFIR<float>::design`, both of which take one by value.
 */
/*
 * The order is READ FROM designWindow's switch, not assumed: case 1 tail-calls
 * `hanning` and case 2 tail-calls `hamming`, which is the opposite way round
 * from the alphabetical guess.  Case 0 and the default both go to `boxcar`.
 */
enum WindowType {
	WINDOW_BOXCAR = 0,
	WINDOW_HANNING = 1,
	WINDOW_HAMMING = 2,
	WINDOW_BLACKMAN = 3
};

/** @brief Sum of @p n elements: Sx. */
template <typename T> T sum(T *x, unsigned n);
/** @brief Arithmetic mean of @p n elements: Sx / n. */
template <typename T> T mean(T *x, unsigned n);
/** @brief Mean of squares of @p n elements: Sx^2 / n. Misnamed by the original -- not a sum. */
template <typename T> T sqrSum(T *x, unsigned n);
/** @brief Population variance, built on sqrSum() and mean(): sqrSum(x) - mean(x)^2. */
template <typename T> T Var(T *x, unsigned n);
/** @brief Standard deviation, built on Var(): sqrt(Var(x)). */
template <typename T> T Std(T *x, unsigned n);
/** @brief sinc(x) = sin(x)/x, with the x == 0 case handled as 1. */
template <typename T> T sinc(T x);

/** @brief Rectangular window: fills @p w (@p n entries) with 1. */
template <typename T> void boxcar(T *w, unsigned n);
/** @brief Hamming window, @p n entries into @p w. */
template <typename T> void hamming(T *w, unsigned n);
/** @brief Hann (raised-cosine) window, @p n entries into @p w. */
template <typename T> void hanning(T *w, unsigned n);
/** @brief Blackman window, @p n entries into @p w. */
template <typename T> void blackman(T *w, unsigned n);

/**
 * @brief Dispatch to one of the four window functions above by enum value.
 *
 * Case order is read from the object's own switch, not assumed: 1 tail-calls
 * hanning(), 2 tail-calls hamming() -- the opposite way round from the
 * alphabetical guess. 0 and any other value go to boxcar().
 *
 * @param t  Which window shape to design.
 * @param w  Output buffer, @p n entries.
 * @param n  Window length.
 */
template <typename T> void designWindow(WindowType t, T *w, unsigned n);

#endif /* DSPLIB_DSPMATH_H */
