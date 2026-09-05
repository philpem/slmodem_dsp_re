/**
 * @file V90BitsToSymbol.h
 * @brief `V90BitsToSymbol`, the V.90 downstream bit-to-symbol converter: it
 *        buffers incoming data bits, hands them to an owned `V90Mapper`, and
 *        assembles the resulting symbols into fixed-size blocks for the
 *        modulator.
 *
 * Nine members and 1,672 bytes of code (each of the duplicated constructor
 * and destructor symbols counted once), all nine now written; `nm -S -C` is
 * where the figures come from.
 *
 * Not polymorphic: `~V90BitsToSymbol` is listed with `D1` and `D2` and no
 * `D0`, so offset 0 is a real member and there is no vptr.
 *
 * The size is 0x24, the original compiler's own `sizeof`: two independent
 * call sites allocate this class and both spell it the same way --
 * `V90Phase4Modulator`'s constructor does `movl $0x24,(%esp) ; call
 * sysdep_malloc ; ... ; call V90BitsToSymbol::C1`, and `V90Modulator`'s does
 * the same (finding F1246). The last field the constructor writes is the
 * byte at +0x20, so the class ends at 0x21 and pads to 0x24.
 *
 * `mapper` is owned and `params` is borrowed: `mapper` at +0x00 is
 * `sysdep_malloc(sizeof(V90Mapper))` followed by `V90Mapper::V90Mapper`, and
 * the destructor destroys and frees it; `params` at +0x04 is stored straight
 * from the argument and the destructor does not touch it. The mapper's
 * second constructor argument is reloaded from `params` at +0x04 rather than
 * kept in the register the argument arrived in (`mov 0x4(%ebx),%edx` after
 * the allocator call), so the original wrote the member and not the
 * parameter there; nothing can distinguish the two by behaviour, so this is
 * reproduced as the blob does it.
 *
 * The field names beyond +0x0c come from `reset` and `nofBitsForNextTime`
 * below, which read and write them, and `bitsPerFrame`/`extraSymbols` are
 * the two fields the constructor leaves untouched until the first `reset` --
 * worth stating because it is exactly what a differential test over
 * never-zeroed storage checks.
 *
 * Data member names are invented; the mangling never carries one (finding
 * F226).
 */

#ifndef DSPLIB_V90BITSTOSYMBOL_H
#define DSPLIB_V90BITSTOSYMBOL_H

#include "dsplib/V90Phase3Modulator.h"	/* for `PcmType`; see the resets */

class V90Mapper;
class V90MappingParams;
class V90Parameters;

class V90BitsToSymbol {
public:
	/**
	 * @brief Construct the converter and its owned mapper.
	 *
	 * Stores @p params, allocates and constructs an owned `V90Mapper`
	 * over it, and allocates the `symbols` output buffer at
	 * `2 * nofSymbols` bytes. `bitsPerFrame` and `extraSymbols` are
	 * deliberately left uninitialized; the first `reset()` fills them in.
	 *
	 * @param nofSymbols  Capacity of the owned symbol buffer, in symbols.
	 * @param params      The V.90 parameter block; borrowed, not owned.
	 */
	V90BitsToSymbol(unsigned int nofSymbols, V90Parameters *params);
	/** @brief Destroy the owned mapper and free the symbol buffer. */
	~V90BitsToSymbol();

	/**
	 * @brief How many bits the caller must supply to fill what remains
	 *        of the current block.
	 *
	 * The symbols still owed are `symbolsBlockSize - symbolsDone`, plus
	 * `extraSymbols` while `extraSymbolsPending` is set; six symbols
	 * make a frame, and each frame costs `bitsPerFrame` bits. The
	 * object computes the two rounding cases (an exact multiple of six,
	 * and a remainder needing one more frame) with different instruction
	 * sequences that agree on every input, and both are reproduced.
	 *
	 * @return The number of bits needed, in `bitsPerFrame` units.
	 */
	unsigned int nofBitsForNextTime();
	/**
	 * @brief Set the target block size and report the bits needed to fill it.
	 * @param blockSize  The new value for `symbolsBlockSize`.
	 * @return `nofBitsForNextTime()`, computed inline.
	 */
	unsigned int setSymbolsBlockSize(unsigned int blockSize);
	/**
	 * @brief Hand out one completed block of symbols.
	 *
	 * Copies out `symbolsBlockSize` symbols (or however many are ready,
	 * on underflow) and shifts whatever is left over down to the front
	 * of the buffer.
	 *
	 * @param nofBits     Receives the bit count for the next call, from
	 *                    nofBitsForNextTime().
	 * @param outSymbols  Receives the block's symbols.
	 * @return 0 on success, 1 if `symbolsBlockSize` is still zero
	 *         ("SIZE_NOT_SET"), 3 if fewer than a full block was ready
	 *         ("BUFFER_UNDERFLOW"). Never 2; only the fill overload below
	 *         can overflow.
	 */
	unsigned int process(unsigned int &nofBits, short *outSymbols);
	/**
	 * @brief Feed data bits into the mapper and append the resulting symbols.
	 *
	 * The fill side of the class: @p bits go to the owned mapper, and
	 * the symbols it produces are appended to `symbols` at
	 * `symbolsDone`. Nothing here bounds the mapper's write against the
	 * `2 * nofSymbols` allocation -- status 2 is a report, not a guard
	 * (finding F7430).
	 *
	 * @param bits     The input data bits.
	 * @param nofBits  How many bits of @p bits are valid.
	 * @return 0 on success, 1 if `symbolsBlockSize` is still zero
	 *         ("SIZE_NOT_SET"), 2 if the append pushed `symbolsDone`
	 *         past `nofSymbols` ("BUFFER_OVERFLOW"). Never 3; only the
	 *         drain overload above can underflow.
	 */
	unsigned int process(unsigned char *bits, unsigned int nofBits);
	/**
	 * @brief The fill and the drain in one call.
	 *
	 * The overload the transmit chain actually calls --
	 * `V90Modulator::progress`'s data phase relocates against exactly
	 * this mangling and neither sibling. It is not a composition of the
	 * two single-direction overloads above: it evaluates fill, overflow
	 * and underflow as exclusive arms of one test over the post-fill
	 * `symbolsDone`, so it is the only member that can report all three
	 * statuses from one call. A short fill is diagnosed before an
	 * overrun, so an underflowing call never reports 2 however far past
	 * the allocation the mapper wrote.
	 *
	 * @param bits        The input data bits.
	 * @param nofBits     In: how many bits of @p bits are valid. Out:
	 *                    the bit count needed for the next call.
	 * @param outSymbols  Receives a completed block's symbols, when one
	 *                    is ready.
	 * @return 0 on success, 1 "SIZE_NOT_SET", 2 "BUFFER_OVERFLOW", or 3
	 *         "BUFFER_UNDERFLOW".
	 */
	unsigned int process(unsigned char *bits, unsigned int &nofBits,
			     short *outSymbols);

	/**
	 * @brief Reset the converter and its mapper for a new spectral-shaped connection.
	 *
	 * Forwards both arguments to `mapper->reset()`, sets `bitsPerFrame`
	 * from the mapping parameters' first word, computes `extraSymbols`
	 * as the number of symbols the mapper's spectral shaper will
	 * suppress while priming (`6 * shaperId / shaperSR`, zero when
	 * `shaperSR` is zero -- finding F7422 has the algebra), and clears
	 * `symbolsDone`/`symbolsBlockSize`/`extraSymbolsPending` as the
	 * constructor does.
	 *
	 * @param mp   The new V.90 mapping parameters.
	 * @param pcm  mu-law or A-law, forwarded to the mapper unchanged.
	 */
	void reset(V90MappingParams *mp, PcmType pcm);
	/**
	 * @brief Reset the converter and its mapper with no spectral shaping.
	 *
	 * Forwards both arguments to `mapper->resetNoSpectral()` and sets
	 * `bitsPerFrame`; unlike reset(), leaves `extraSymbols` and the
	 * three block-position fields untouched.
	 *
	 * @param mp   The new V.90 mapping parameters.
	 * @param pcm  mu-law or A-law, forwarded to the mapper unchanged.
	 */
	void resetNoSpectral(V90MappingParams *mp, PcmType pcm);

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	V90Mapper *mapper;		/* +0x00 owned, 0x704 bytes         */
	V90Parameters *params;		/* +0x04 borrowed                   */
	short *symbols;			/* +0x08 owned, 2 * nofSymbols      */
	unsigned int nofSymbols;	/* +0x0c the first argument         */
	unsigned int symbolsDone;	/* +0x10 zeroed by ctor and reset   */
	unsigned int bitsPerFrame;	/* +0x14 NOT set by the constructor */
	unsigned int extraSymbols;	/* +0x18 NOT set by the constructor */
	unsigned int symbolsBlockSize;	/* +0x1c zeroed by ctor and reset   */
	unsigned char extraSymbolsPending; /* +0x20 set to 1 by both         */
	/*
	 * +0x21 was `pad_21[3]`, the struct's LAST member -- REMOVED
	 * (finding F10151).  Already correctly described as tail padding;
	 * `extraSymbolsPending` ends at +0x21 and the class's own 4-byte
	 * alignment rounds `sizeof` up to +0x24 on its own, already proved
	 * by the existing `v90bts_size[(sizeof(V90BitsToSymbol) == 0x24) ?
	 * 1 : -1]` hard compile assertion. `dis.py` over all eleven
	 * `V90BitsToSymbol` methods finds no access to 0x21/0x22/0x23.
	 */
};

#endif /* DSPLIB_V90BITSTOSYMBOL_H */
