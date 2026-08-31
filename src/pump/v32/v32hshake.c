/*
 * v32hshake.c -- ITU-T V.32 / V.32bis: one block of the half-duplex handshake.
 *
 * Reconstructed from dsplibs.o:
 *
 *   v32_handshake   .text 0x082b00   203
 *
 * This is the drivers' only caller.  It clears an event bit, copies the
 * caller's transmit words into the context's own buffer, runs the receive
 * state once, and TAIL-CALLS the transmit driver.
 *
 * IT IS ITS OWN FILE, and that is deliberate.  In the object it lives at
 * 0x082b00 and `V32TxHdxModem` lives at 0x07fce0 -- different translation
 * units (`V32mod.c` and `V32TXHDX.c`; finding F8201 reads the boundary).
 * Appending it to `v32hdx.c` would put two of the author's files in one of
 * ours, and an inline definition's POSITION in a translation unit is a
 * lever-3 carrier: finding F7815 cost eight destructors their byte identity
 * for exactly that move, and F7796 gained seventeen symbols by undoing it.
 *
 * ---------------------------------------------------------------------------
 * THE ARGUMENT LIST IS SEVEN LONG AND INTERLEAVED, AND THE CALLEES TYPE IT
 *
 * Nothing that calls `v32_handshake` is reconstructed yet -- `v32_process` is
 * unwritten -- so the names below come from the two CALLEES, which is grade 2
 * evidence and the strongest available here.  The frame is four pushes and
 * `sub $0x2c`, so the arguments start at 0x40(%esp):
 *
 *   +0x40  modem      -> both callees' first argument
 *   +0x44  txdata     -> copied into hdx + 0xa4, `symbol_len` words
 *   +0x48  txout      -> V32TxHdxModem's `out`
 *   +0x4c  rxin       -> V32RxHdxModem's `in`
 *   +0x50  rxout      -> V32RxHdxModem's `out`
 *   +0x54  nsamples   -> V32TxHdxModem's `nsamples`
 *   +0x58  rxcount    -> V32RxHdxModem's `count`
 *
 * The receive arguments and the transmit arguments ALTERNATE rather than
 * grouping, which is why the object spills six of the seven to its own frame
 * before using any: the two calls want them in an order the caller's order
 * does not supply.  A reading that grouped them would still compile and would
 * still pass a fixture that made the buffers alias.
 *
 * ---------------------------------------------------------------------------
 * THE TRANSMIT DATA IS COPIED, NOT PASSED
 *
 * `V32TxHdxModem` is reached by a sibling `jmp` at 82bbb and its second
 * argument is set at 82ba6 from `hdx + 0xa4` -- the context's OWN buffer,
 * re-read from `obj + 0x64` AFTER the receive state has run.  So the caller's
 * `txdata` never reaches the transmit driver: it is copied in first, and if
 * the receive state replaced the whole context, the copy the transmit driver
 * sees is the NEW context's buffer and not the one just written.  That is a
 * real ordering and not an artefact; the copy happens before the receive call
 * and the pointer is taken after it.
 *
 * THE COPY'S LENGTH IS A `short` AND ITS INDEX WRAPS.  The loop counter is
 * kept sixteen bits wide throughout -- `lea 0x1(%edx),%eax` then
 * `movswl %ax,%edx` -- and the bound is `cmp %dx,0x9e(%ecx)`, a SIGNED 16-bit
 * compare against `hdx->symbol_len`.  The guard at 82b4c is `jle`, also
 * signed, so a negative `symbol_len` copies nothing.
 *
 * THE ELEMENT LOAD IS `movzwl` AND THE STORE IS SIXTEEN BITS, so the extension
 * is dead and finding F614 says the FIELD's type is not what varies.  Per
 * F7803 it follows the declared type of the LOCAL being loaded into, and
 * `unsigned short` is what produces it.  This is the first evidence anywhere
 * about the element type of hdx + 0xa4, which finding F8239 listed as one of
 * four things left unsettled -- and it is evidence about a LOCAL, so 8239's
 * question is still open.  What can be said is that the buffer is copied as
 * sixteen-bit words and never interpreted here.
 *
 * ---------------------------------------------------------------------------
 * THE TWO FLAG CLEARS
 *
 * Bit 0x01 of obj + 0x31 is cleared on every call.  Bits 0x04 and 0x08 are
 * cleared as well, and ONLY when `hdx->mode` is 4 -- the `V32RngInitNextState`
 * slot of `V32NextState`.  The object spells this as two paths that converge
 * on one store, so the `& 0xfe` is applied to whichever value the mode test
 * selected:
 *
 *      82b2e  cmpw $0x4,0x76(%ecx)      hdx->mode
 *      82bc0  movzbl 0x31(%esi),%eax  ; and $0xf3,%al   ->  falls into
 *      82b45  and $0xfe,%al           ; mov %al,0x31(%esi)
 *
 * Finding F8239 owns three of the eight bits of obj + 0x31 and names 0x20 and
 * 0x40; 0x01, 0x04 and 0x08 are not among the named ones, and nothing here
 * tells us what they indicate.  They are therefore spelled as constants
 * wearing their own values -- the flag-side equivalent of `short_2800` -- and
 * not given roles.  Naming one wrongly is worse than leaving it numbered.
 */

#include "dsplib/v32hdxst.h"
#include "dsplib/v32fpctl.h"

/*
 * MODELLED, UNNAMED.  See the header comment: the object clears these three
 * bits here and nothing in the object says what any of them indicates.
 * V32_FLAG_FAULT (0x02), V32_FLAG_CARRIER (0x20) and V32_FLAG_SILENCE (0x40)
 * are the three of the eight that ARE named, and they are elsewhere.
 */
#define V32_FLAG_01		0x01
#define V32_FLAG_04		0x04
#define V32_FLAG_08		0x08

/* hdx + 0xa4, the transmit buffer the context owns.  v32fpctl.h's name. */
#ifndef V32_HDX_BUF_A4
#define V32_HDX_BUF_A4		0xa4
#endif

#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_S16(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_U8(obj, off)	(*(unsigned char *)FIELD((obj), (off)))

void
v32_handshake(void *modem, unsigned short *txdata, short *txout, short *rxin,
	      unsigned short *rxout, short *nsamples, unsigned short *rxcount)
{
	unsigned char *hdx;
	unsigned char flags;
	short i;

	hdx = (unsigned char *)FIELD_PTR(modem, V32_OBJ_HDX);

	flags = FIELD_U8(modem, V32_OBJ_FLAGS);
	if (FIELD_S16(hdx, V32HDX_MODE) == V32_MODE_RING_INIT)
		flags = (unsigned char)(flags & ~(V32_FLAG_04 | V32_FLAG_08));
	FIELD_U8(modem, V32_OBJ_FLAGS) = (unsigned char)(flags & ~V32_FLAG_01);

	if (FIELD_S16(hdx, V32HDX_SYMBOL_LEN) > 0) {
		unsigned short *buf;

		buf = (unsigned short *)FIELD_PTR(hdx, V32_HDX_BUF_A4);
		i = 0;
		do {
			buf[i] = txdata[i];
			i = (short)(i + 1);
		} while (FIELD_S16(hdx, V32HDX_SYMBOL_LEN) > i);
	}

	V32RxHdxModem(modem, rxin, rxout, rxcount);

	/*
	 * RE-READ, and it matters: the receive state may have replaced the
	 * whole context, exactly as `V32TxHdxModem`'s own loop allows.
	 */
	hdx = (unsigned char *)FIELD_PTR(modem, V32_OBJ_HDX);
	V32TxHdxModem(modem, (short *)FIELD_PTR(hdx, V32_HDX_BUF_A4), txout,
		      nsamples);
}
