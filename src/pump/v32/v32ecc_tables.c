/*
 * v32ecc_tables.c -- V.32's echo canceller configuration and its two maps.
 *
 * Reconstructed from dsplibs.o:
 *   ECCv32_QMAP .rodata 0x07058  0x18
 *   ECCv32_IMAP .rodata 0x07070  0x18
 *   ECCv32_CFG  .rodata 0x07088  0x18
 *
 * `ECCv32_IMAP` AND `ECCv32_QMAP` ARE NOT COEFFICIENT TABLES.  Each is six
 * POINTERS, and the six are resolved from the object's relocations rather
 * than from the bytes -- the bytes are zero, and reading them as data is
 * exactly the mistake `tools/dis.py` exists to prevent.  In order:
 *
 *     [0] SMCv32_IMAP16   [1] SMCv32_IMAP16   [2] VTBv32_IMAP32
 *     [3] VTBv32_IMAP16T  [4] VTBv32_IMAP64   [5] VTBv32_IMAP128
 *
 * and the same six spellings with `Q`.  Indices 0 and 1 are the SAME table,
 * measured and not inferred from equal contents: they are two relocations
 * against one symbol.  The index is the V.32 rate code, so 4800 and 9600
 * share the sixteen-point map and the four trellis rates take one each.
 *
 * THIS FILE COULD NOT EXIST UNTIL THE VITERBI BATCH DID.  Finding F1614
 * records why: four of the ten pointees are `VTBv32_*`, the harness renames
 * every symbol the blob defines, and an `extern` against a renamed symbol
 * resolves to nothing -- so there was no way to declare the pointer arrays
 * without owning the data.  The `VTBv32_*` maps landed in this session's
 * Viterbi batch and the `SMCv32_*` pair was already in v32dec_tables.c, so
 * the ten pointees are all real definitions now.
 *
 * ELEMENT WIDTH of the pointees is `short`, from `FPM_ECC_cancel`, which
 * indexes one with `movswl (%ecx,%eax,2)`; `SMCv32_IMAP16`'s 0x22 bytes are
 * therefore 17 entries and not 16.
 *
 * `ECCv32_CFG`'s field boundaries are not a reading of the bytes either --
 * they are what `FPM_ECC_init`'s disassembly does with them (finding F1611),
 * and the two pointer fields are the only two relocations in the object.
 * const because it is `.rodata`, where `ECC_CFG` is `.data` and is not.
 */

#include "dsplib/fpm_ecc.h"
#include "dsplib/v32dec.h"
#include "dsplib/vtb.h"

const short *const ECCv32_IMAP[6] = {
	SMCv32_IMAP16, SMCv32_IMAP16, VTBv32_IMAP32,
	VTBv32_IMAP16T, VTBv32_IMAP64, VTBv32_IMAP128
};

const short *const ECCv32_QMAP[6] = {
	SMCv32_QMAP16, SMCv32_QMAP16, VTBv32_QMAP32,
	VTBv32_QMAP16T, VTBv32_QMAP64, VTBv32_QMAP128
};

const struct fpm_ecc_cfg ECCv32_CFG = {
	480,		/* far_lag   */
	40,		/* near_taps */
	40,		/* far_taps  */
	0,		/* pad06     */
	ECCv32_IMAP,	/* imap      */
	ECCv32_QMAP,	/* qmap      */
	16,		/* fill      */
	0,		/* pad12     */
	0		/* aux       */
};
