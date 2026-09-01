/*
 * fpm_fsm_cfg.c -- Fixed Point Modem: the FSK Modulator's library built-in
 *                  configuration, `D` at .data:0x8198, 8 bytes.
 *
 * THE OBJECT'S OWN `FPM_FSM_CFG` IS HERE, AND IT IS NOT `FPM_FSM_CFG_data`.
 * This is `fpm_fsd_cfg.c`'s situation a third time (that file's own header
 * names it "FPM_MTD_CFG's situation a second time"): when `src/dsp/fpm_fsm.c`
 * was written the blob symbol had no caller that needed it, so a stub was
 * given its own `_data` name and `FPM_FSM_CFG` was left unwritten.
 * `V21TX_create` (0x0992f0) references `FPM_FSM_CFG` directly -- two loads at
 * 0x0993e2 and 0x0993e8 that copy it onto the stack before `FPM_FSM_init` --
 * and a `src/` reference to an unwritten blob symbol cannot link (F8492).  So
 * it has to exist under the object's name.  Finding F9356.
 *
 * THE TWO ARE THE SAME BYTES.  The object's eight bytes at .data:0x8198 are
 * `3a 07 72 06 18 00 ff 7f`, which is `{freq: {1850, 1650}, samples_per_sym:
 * 24, scale: 32767}` -- exactly what `src/dsp/fpm_fsm.c`'s `FPM_FSM_CFG_data`
 * already carries, and its own comment already read the value off the same
 * bytes ("V.21 channel 2 at full scale").  `t_v21txcreate.c` asserts the two
 * are `memcmp`-identical rather than assuming it.
 *
 * What remains is a storage class and a duplicate symbol: the object's is
 * `D`, global and writable; the stub is `const`, in `.rodata`.
 *
 * NOT FIXED HERE, AND THE FIX IS ONE LINE THIS PASS CANNOT REACH.  Delete the
 * stub and its declaration in `include/dsplib/fpm_fsm.h`, and point its one
 * reader -- `src/pump/b103/b103fp.c:1092`, `fsm = FPM_FSM_CFG_data;` -- at
 * `FPM_FSM_CFG`.  That file is `src/pump/**`, fenced from this pass exactly as
 * it was from `fpm_fsd_cfg.c`'s.  Recorded as D1180's shape, D1230.
 */

#include "dsplib/fpm_fsm.h"

/*
 * `D` in the object -- global and writable -- hence not `const`.
 *
 * V.21 channel 2's mark and space (see `v21cfg.h`'s `V21_CHAN2_MARK_HZ` /
 * `V21_CHAN2_SPACE_HZ`), full symbol length for 300 baud at 7200 Hz, and full
 * scale.  `V21TX_create` overrides `scale` to 0x1900 (6400) after copying
 * this in; every other caller gets it unpatched.
 */
struct fpm_fsm_cfg FPM_FSM_CFG = {
	{ 1850, 1650 },	/* +0x00 freq: space, mark                  */
	24,		/* +0x04 samples_per_sym                    */
	32767		/* +0x06 scale                               */
};
