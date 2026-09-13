/*
 * v32hdx.h -- ITU-T V.32 / V.32bis: the two drivers of the half-duplex machine.
 *
 *   V32TxHdxModem   -> calls the current TRANSMIT state until the block's
 *                      symbol budget is spent
 *   V32RxHdxModem   -> tail-calls the current RECEIVE state, once
 *
 * Both are dispatchers and nothing else.  Their whole content is the CONTRACT
 * they impose on the twenty states around them, which is why this header is
 * mostly prose: the next V.32 pass writes those states against it.
 *
 * THE INSTANCE IS NOT MODELLED; the parameter is `void *` and the offsets are
 * named constants, following `include/dsplib/v32data.h`'s ruling, which in
 * turn follows `include/dsplib/v22data.h`'s.  Do not turn this into a struct.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE'S TWO POINTERS
 *
 *   obj + 0x64   the half-duplex / handshake context -- `hdx` throughout
 *   obj + 0x68   the DSP / datapump block -- `fp`, already V32_OBJ_FP
 *
 * ---------------------------------------------------------------------------
 * WHERE +0x9e AND +0xa0 COME FROM, WHICH IS THE AUTHOR'S OWN WORD
 *
 * Both are written twice, and both times from a table this object names:
 *
 *   7f136  V32FP_recreate   hdx + 0x9e = V32_SYMBOL_LEN[rate]
 *   7f149  V32FP_recreate   hdx + 0xa0 = V32_SAMPLE_LEN[rate]
 *   84619  V32FP_control    hdx + 0x9e = V32_SYMBOL_LEN[rate]
 *   8462c  V32FP_control    hdx + 0xa0 = V32_SAMPLE_LEN[rate]
 *
 * So +0x9e is SYMBOLS per block and +0xa0 is SAMPLES per block, and the two
 * are not interchangeable even though a fixture that sets them equal cannot
 * tell them apart.  `V32FP_control` also fills hdx + 0x84 from
 * `V32_SYMBOL_LEN` at 84606, so +0x9e is a second copy rather than the only
 * one; which of the two a state should read is that state's question.
 *
 * ---------------------------------------------------------------------------
 * THE SIGNEDNESS OF +0x9e, WHICH IS TWO ANSWERS AND NOT A CONTRADICTION
 *
 * `V32TxHdxModem` loads it with `movzwl` and discards the upper half -- the
 * value is stored straight back as sixteen bits.  `RxHdxNull` (844af) loads
 * the SAME field with `movswl` and adds the 32-bit result to an `int`.
 *
 * Per finding F614 the second is FORCED and the first is not, and per finding
 * F7803 a dead extension follows the declared type of the LOCAL being loaded
 * into rather than the field's.  So:
 *
 *   the FIELD at hdx + 0x9e is `short`      -- RxHdxNull's use forces it
 *   the LOCAL in V32TxHdxModem is `unsigned short`
 *
 * and the local's type is confirmed independently, by the state functions:
 * `RxHdxNull` reads its fourth argument with `movzwl (%ebx),%edx`, so the
 * pointer the drivers hand the states is `unsigned short *`.  The two
 * readings agree over every count `V32_SYMBOL_LEN` can hold, so no test can
 * separate them; the object was not free to choose either instruction.
 *
 * ---------------------------------------------------------------------------
 * THE CONTRACT A TRANSMIT STATE IS WRITTEN TO -- read this before writing one
 *
 * `V32TxHdxModem` seeds a local from hdx + 0x9e and loops:
 *
 *      left = hdx->symbol_len;
 *      do {
 *              hdx = obj->hdx;                 <-- RELOADED every iteration
 *              n = (*hdx->txstate)(obj, data, out, &left);
 *              total += n;                     <-- truncated to short
 *              out   += n;                     <-- short *, so 2*n bytes
 *      } while (left != 0);
 *      *nsamples = total;
 *
 * Four things follow, and all four are load-bearing for a state:
 *
 *   1. `left` is IN/OUT and is the loop's only exit.  A state that does not
 *      reduce it spins for ever.  It is a SYMBOL budget, not a flag.
 *   2. `hdx` is re-read from obj + 0x64 at the top of every iteration, so a
 *      state may replace the whole context and the next iteration sees it.
 *   3. The state pointer is re-read too, so a state may install its successor
 *      at hdx + 0x6c and that successor runs on the next iteration of the
 *      SAME call.  This is how a transition mid-block works.
 *   4. The return is the SAMPLE count the state wrote, and it is accumulated
 *      into a `short`.  `TxHdxTone` (7fd88, 7fda5) returns `hdx->sample_len`
 *      on BOTH of its exits -- read after any transition, not before -- and
 *      so do `TxHdxNull`'s three exits (802bf, 802f2, 80318).
 *
 * A state transitions through `call *V32NextState[hdx->mode]`: 7fd9b is
 * `TxHdxTone`'s.  `V32NextState` is 24 bytes -- six slots -- and the object
 * relocates five of them:
 *
 *      0  V32OrgNextState        3  V32LocLoopNextState
 *      1  V32AnsNextState        4  V32RngInitNextState
 *      2  V32LocLoopNextState    5  (no relocation)
 *
 * which is what types hdx + 0x76 as the MODE.  hdx + 0x74 is the separate
 * 0..34 handshake state of `v32state.h`: `RxHdxNull` writes 0x21 there while
 * installing `RxHdxError`, and 0x21 is `V32_STATE_ERROR`.
 *
 * ---------------------------------------------------------------------------
 * THE CONTRACT A RECEIVE STATE IS WRITTEN TO
 *
 * `V32RxHdxModem` is four instructions and passes its own frame through
 * untouched, so its arity is invisible in its own twelve bytes and comes
 * from the states instead.  `RxHdxNull` (844a0) settles it: four arguments,
 * the fourth an `unsigned short *` read on entry and written on exit.
 * `RxHdxSTone` (84345) hands its second and third arguments straight to
 * `DemodDataV32` as that function's second and third, and `*count` as its
 * count, then stores the return back through the same pointer -- so the
 * receive state and `DemodDataV32` share one argument list, and `v32demod.h`
 * types it.
 *
 * The receive driver does NOT loop.  One state, one call, and the state owns
 * whatever iteration it needs.
 */

#ifndef DSPLIB_V32HDX_H
#define DSPLIB_V32HDX_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The half-duplex / handshake context.  V32_OBJ_FP (0x68) is v32data.h's.
 *
 * GUARDED because `v32seq.h` defines the same offset from its own reading,
 * and the two headers have to be includable in one translation unit.
 */
#ifndef V32_OBJ_HDX
#define V32_OBJ_HDX		0x64
#endif

#define V32HDX_TXSTATE		0x6c	/* v32_txhdx_fn, current transmit state */
#define V32HDX_RXSTATE		0x70	/* v32_rxhdx_fn, current receive state  */
#define V32HDX_MODE		0x76	/* short, indexes V32NextState          */
#define V32HDX_STATE		0x74	/* short, v32state.h's 0..34            */
#define V32HDX_SYMBOL_LEN	0x9e	/* short, = V32_SYMBOL_LEN[rate]        */
#define V32HDX_SAMPLE_LEN	0xa0	/* short, = V32_SAMPLE_LEN[rate]        */

/*
 * A transmit state: write samples into `out`, reduce `*left` by the symbols
 * consumed, and return the number of SAMPLES written.
 *
 * `data` is hdx + 0xa4, a buffer the context owns -- `v32_handshake` (82ba6)
 * loads it and passes it straight through.  Its element type is not
 * established by either driver, and `short *` here is the shape the output
 * pointer forces on its sibling rather than a reading of the states.
 */
typedef short (*v32_txhdx_fn)(void *modem, short *data, short *out,
			      unsigned short *left);

/*
 * A receive state.  The argument list is `DemodDataV32`'s with the count
 * passed by pointer instead of by value; see v32demod.h for the two buffers.
 *
 * `void` and not `short`: `V32RxHdxModem` tail-jumps to it and its own caller
 * `v32_handshake` (82b93) discards whatever comes back, so nothing in the
 * object claims a value.  `RxHdxNull` leaves `RxClampV32`'s return in %eax
 * incidentally, which is not a promise.
 */
typedef void (*v32_rxhdx_fn)(void *modem, short *in, unsigned short *out,
			     unsigned short *count);

/**
 * @brief Drive the V.32 half-duplex TRANSMIT machine for one block.
 *
 * Runs transmit states until the block's symbol budget (hdx + 0x9e) is
 * spent, writing the total sample count through @p nsamples. See the file
 * banner for the full loop contract: `hdx` and the current state are
 * re-read on every iteration, so a state may install its successor and
 * even replace the whole context mid-block.
 *
 * Declared `void` and not `short`: nothing in the object sets `%eax`
 * before the `ret`, and the result is delivered through the fourth
 * argument instead. The one caller, `v32_handshake` (82bbb), reaches it by
 * a sibling `jmp` and so cannot discriminate.
 *
 * @param modem     The V.32 datapump instance.
 * @param data      Scratch data buffer the current transmit state reads/writes.
 * @param out       Output for the modulated transmit samples.
 * @param nsamples  Output: the total sample count produced this block.
 */
void V32TxHdxModem(void *modem, short *data, short *out, short *nsamples);

/**
 * @brief Drive the V.32 half-duplex RECEIVE machine for one block.
 *
 * Tail-calls the current receive state once; the receive driver does not
 * loop, unlike V32TxHdxModem().
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Received samples to demodulate.
 * @param out    Output for the demodulated bits/symbols.
 * @param count  In/out: input sample count, then output count (the current state's own contract).
 */
void V32RxHdxModem(void *modem, short *in, unsigned short *out,
		   unsigned short *count);

/**
 * @brief One block of the V.32 half-duplex handshake: the two drivers' only caller.
 *
 * Clears an event bit, copies @p txdata into the context's own buffer,
 * runs the receive state once, then tail-calls V32TxHdxModem() on the
 * buffer.
 *
 * The seven arguments alternate between the two callees rather than
 * grouping, which is what the object's six spills at 82b17..82b2a encode;
 * the order here is read off those and off the two calls, which is
 * grade-2 evidence (a callee that types it) because nothing calling this
 * function is written. See `src/pump/v32/v32hshake.c`.
 *
 * @param modem     The V.32 datapump instance.
 * @param txdata    Transmit symbols/data.
 * @param txout     Output for the modulated transmit samples.
 * @param rxin      Received samples to demodulate.
 * @param rxout     Output for the demodulated receive data.
 * @param nsamples  Output: transmit sample count (from V32TxHdxModem()).
 * @param rxcount   In/out: receive sample/symbol count (the current receive state's contract).
 *
 * FILE-LOCAL IN THE OBJECT, so it is `static` in `src/pump/v32/v32fpdisp.c`,
 * the unit that holds the `V32_PROTOCOL` table which names it, and has no
 * declaration here.  A test names it through the test tier's globalized copies
 * (tools/testvisible.py).
 */

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32HDX_H */
