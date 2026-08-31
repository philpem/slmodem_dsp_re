/*
 * v32hdx_tables.c -- the four `.data` tables the V.32 half-duplex machine
 *                    dispatches and configures itself from.
 *
 * Extracted from dsplibs.o with tools/tabdump.py:
 *
 *   V32NextState   .data 0x0076cc   24   GLOBAL   six function pointers
 *   V32_CONNECT    .data 0x007724   14   GLOBAL   seven shorts
 *   V32_RX_MODE    .data 0x007732   14   GLOBAL   seven shorts
 *   V32_TX_MODE    .data 0x007740   14   GLOBAL   seven shorts
 *
 * ALL FOUR ARE `.data` AND NOT `.rodata`, so none of them is `const`.  That
 * is the object's placement and not a preference: section 143 is `.data` and
 * section 129 is `.rodata`, and V.32's other tables -- `V32_S_DATA_COEF`,
 * `SREv32_COFFS`, `V32DiconnectThreshTable` -- are in 129.  Making these
 * const would move them and cost byte identity in whatever translation unit
 * the original put them in.
 *
 * ---------------------------------------------------------------------------
 * V32NextState, AND WHY IT IS READ FROM THE RELOCATIONS
 *
 * The six dwords are ZERO in the file.  Their values are relocations, so
 * `objdump` prints a table of small integers and any tool that trims its
 * output loses them entirely -- the trap `tools/dis.py` exists for, and
 * `tabdump.py` prints them for a data range:
 *
 *     +0x00  V32OrgNextState        +0x0c  V32LocLoopNextState
 *     +0x04  V32AnsNextState        +0x10  V32RngInitNextState
 *     +0x08  V32LocLoopNextState    +0x14  V32RngRespNextState
 *
 * `V32LocLoopNextState` really does appear twice, at slots 2 and 3.  See
 * finding F8560 for the correction this makes to `v32hdx.h`'s prose.
 *
 * ---------------------------------------------------------------------------
 * THE THREE SHORT TABLES
 *
 * Seven entries each, which is `V32_RATE_COUNT` from `v32seq.h` -- the six
 * V.32bis rates plus `V32_RATE_NONE`.  `V32_RX_MODE` and `V32_TX_MODE` hold
 * 1..7 identically; they are separate objects in the blob with separate
 * symbols and separate reference sites, so they are separate objects here
 * even though their contents coincide today.
 *
 * `V32_CONNECT`'s entries are {4, 3, 25, 24, 26, 27, 14}, and THEY ARE NOT
 * HANDSHAKE STATE NUMBERS.  This comment said they were, on the strength of
 * their falling inside `v32state.h`'s 0..34 -- and that was a coincidence.
 * All five reference sites in the object load an entry with `movzwl` and
 * store its LOW BYTE into obj + 0x30, which `v32fpctl.h` names
 * `V32_OBJ_STATUS` and describes as a reason code:
 *
 *     82d99  V32OrgNextState      853c3  V32RngRespNextState
 *     84d16  V32RngInitNextState  85c12  V32AnsNextState
 *     86591  V32LocLoopNextState
 *
 * Nothing copies one into hdx + 0x74.  The same status byte receives 0x0f,
 * 0x16 and 0x17 from `V32AnsNextState`, 0x0e from `V32RngInitNextState` and
 * 0x10, 0x11 and 0x12 from three receive states, none of which is a state
 * number either.  So the entries are per-RATE status codes, the table's own
 * name is the only evidence about what they mean, and it says "connect".
 * Finding F8585.
 *
 * The `movzwl` extension is DEAD -- only %al is used -- so per finding F614 it
 * says nothing about the element type; `short` stands on the symbol's
 * fourteen bytes over seven rates.
 */

#include "dsplib/v32hdxst.h"

v32_nextstate_fn V32NextState[V32_NEXTSTATE_COUNT] = {
	V32OrgNextState,	/* 0  V32_MODE_ORIGINATE  */
	V32AnsNextState,	/* 1  V32_MODE_ANSWER     */
	V32LocLoopNextState,	/* 2  V32_MODE_LOCLOOP_2  */
	V32LocLoopNextState,	/* 3  V32_MODE_LOCLOOP_3  */
	V32RngInitNextState,	/* 4  V32_MODE_RING_INIT  */
	V32RngRespNextState	/* 5  V32_MODE_RING_RESP  */
};

short V32_CONNECT[7] = {
	4, 3, 25, 24, 26, 27, 14
};

short V32_RX_MODE[7] = {
	1, 2, 3, 4, 5, 6, 7
};

short V32_TX_MODE[7] = {
	1, 2, 3, 4, 5, 6, 7
};

/*
 * V32_S_DATA_COEF -- .rodata 0x006d60, 30 bytes, GLOBAL, const.
 *
 * Three biquad sections of five coefficients, and the shape is the CALLEE'S
 * and not this table's: `struct fpm_mtd_cfg::coeff` is `const short *` and
 * `FPM_MTD_create` copies the config wholesale, so the element type and the
 * sectioning come from `include/dsplib/fpm_mtd.h` rather than from a reading
 * of the bytes.
 *
 * The tone it detects is not named here.  `RxHdxSTone` uses the identity of
 * the POINTER, not the values, to tell this bank from `V32_S_COEF` at
 * 0x006d7e, and nothing in the object prints either name -- so "the DATA
 * mode's S tone" is what the symbol itself says and everything past that
 * would be usage inference about a resonator's centre frequency.
 */
const short V32_S_DATA_COEF[15] = {
	-15099, 15735, 27242, -27254, 15735,
	-15099, 15741,     0,      0, 15741,
	-15099, 15735, -27242, 27254, 15735
};
