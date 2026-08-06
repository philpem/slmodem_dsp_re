/*
 * v34hstxblock.h -- `v34handshak`'s once-per-block transmit supervisor.
 *
 * `v34handshak` is 61,541 bytes and three concurrent state machines behind
 * four guards (docs/v34handshak.md).  This is ONE of its four dispatches, and
 * it is reconstructed on its own because the harness in
 * test/harness/v34hsstep.h can put the object into exactly this dispatch and
 * step the function once.
 *
 * WHAT THIS FUNCTION IS.  It is what `v34handshak` does from the moment its
 * guards have selected the txstate table at `.rodata+0x2ee8` -- that is, the
 * seventy-entry dispatch at 0x62b00, its seven distinct targets, and the
 * shared tail at 0x62a40 that every one of them falls into, down to the
 * `ret`.  Over that domain it is the WHOLE of `v34handshak`: the guards read
 * three halfwords and branch, and nothing else in the function runs.
 *
 * WHAT IT IS NOT.  It is not `v34handshak`, and it does not check the guards.
 * Three routes reach this dispatch and all three are the caller's business:
 *
 *      [obj+0x221c] >= [obj+0x2aa0]  and  [obj+0x264] <= 5      0x62ae3
 *      ... and [obj+0x264] > 5, rxstate < 43 and not 4 or 35    0x62a2a
 *      ... and rxstate > 43 and not 53 or 72                    0x62b83
 *
 * All three arrive at 0x62af1 with the same two registers holding the same
 * two values, so the three are one entry point; finding 361 measures that
 * rather than assuming it.
 *
 * THE DOMAIN IS CLOSED, which is why this could be taken as a batch at all.
 * The seven targets branch out of the table's own address range five times --
 * 0x6778b, 0x655c9, 0x67d48, 0x6780f and 0x64884 -- and every one of those
 * five is a handful of instructions that returns to the table's arms or to
 * the shared tail.  Nothing here calls anything, and nothing here traces.
 * Finding 360.
 */

#ifndef DSPLIB_V34HSTXBLOCK_H
#define DSPLIB_V34HSTXBLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

struct v34_object;

/*
 * One pass of the once-per-block transmit supervisor.  `obj` is the V.34
 * object; the txstate halfword at +0x3596 selects the arm.
 */
void v34handshak_txblock(struct v34_object *obj);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34HSTXBLOCK_H */
