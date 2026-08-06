/*
 * v34hstxblock.c -- `v34handshak`'s once-per-block transmit supervisor.
 *
 * See include/dsplib/v34hstxblock.h for what this is and is not, and
 * docs/v34handshak.md for the harness that makes one dispatch of a
 * 61,541-byte function a committable unit.
 *
 * THE SHAPE OF IT.  Seven distinct targets over txstates 5..74, and a shared
 * tail every one of them falls into:
 *
 *      .rodata+0x2ee8[txstate - 5], seventy entries, read with their
 *      relocations attached (finding 360):
 *
 *        0x64480   5                       SILENCE
 *        0x64518   18 19                   SSEG SBARSEG
 *        0x64509   20 21 64 68             PPSEG TRNSEG4 JTXMIT J1TXMIT
 *        0x644c9   24 51 54 60 74          TX_DPSK TX_L1 SILENCEINFO
 *                                          TONE_AB SILENCERETRAIN
 *        0x644fa   66 67 69                TRNSEG4A XMITMP EXMIT
 *        0x644d8   70                      DATAXMIT
 *        0x62a40   the other fifty-four, and every txstate outside 5..74
 *
 * Six of the seven set the int at +0x0004 and fall into the seventh, which is
 * both the default arm and the shared tail.  So the tail runs on every path
 * and the six arms are, between them, a decision about ONE field.
 *
 * WHAT +0x0004 IS.  `struct v34_object`'s own note calls it "an int the shell
 * polls"; the shell's writers put 5, 6 and 10 in it.  This dispatch is the
 * other writer, and the eleven values it can leave are 0, 1, 2, 3, 4, 6, 7,
 * 8, 9, 0xd, 0xf and 0x10.  Nothing reconstructed reads it, so that is all
 * this file claims about it -- the names below are offsets, not meanings.
 *
 * NOTHING HERE CALLS ANYTHING AND NOTHING HERE TRACES.  The whole closure --
 * seven arms, five out-of-range branches and the tail -- holds no `call`
 * instruction and no diagnostic site, which is why every table-2 case prints
 * zero lines on both sides and why the transcript axis of the harness's
 * comparison contributes nothing here.  Finding 362 records that as a gap in
 * the evidence rather than as a passing check.
 */

#include "dsplib/v34fsk.h"
#include "dsplib/v34hstxblock.h"
#include "dsplib/v34recv.h"

/*
 * The fields this dispatch touches that `struct v34_object` does not name.
 *
 * Spelled as offsets rather than added to the header: four agents are
 * reconstructing `v34handshak` cases in parallel and the three state words
 * are read by all of them, so a field added here is a merge conflict in the
 * one header every one of them includes.  `src/pump/v34/v34hshak.c` already
 * spells the same three out this way.
 */
#define TB_MICROSTATE	0x3592	/* short */
#define TB_RXSTATE	0x3594	/* short */
#define TB_TXSTATE	0x3596	/* short */
#define TB_RECEIVER	0x0264	/* struct v34_receiver */
#define TB_F0234	0x0234	/* int  -- the tail's own counter          */
#define TB_F0238	0x0238	/* int  -- the sample-clock running count   */
#define TB_F023C	0x023c	/* int  -- compared against it, unsigned    */
#define TB_F0E4C	0x0e4c	/* unsigned short                           */
#define TB_F2218	0x2218	/* int  -- selects four of the tail's arms  */

#define TB_INT(o, off)	(*(int *)((char *)(o) + (off)))
#define TB_USHORT(o, off) (*(unsigned short *)((char *)(o) + (off)))

/*
 * The shared tail at 0x62a40, which is also the table's default arm.
 *
 * `txstate` is the value the dispatch was entered with; the object keeps it
 * in %cx across every arm and no arm writes +0x3596, so the reload at 0x62b5f
 * cannot change it and is not modelled.
 *
 * TWO SPELLINGS OF ONE VALUE.  The object compares %cx against 0x4a
 * zero-extended (0x62a70) and sign-extends it before the 0x52/0x53/0x54
 * chain (0x62ac5).  One `short` models both, and over the WHOLE halfword
 * range rather than only over the 0..86 the diagnostics allow: every test the
 * two spellings feed is equality against a small positive constant, which
 * agrees for all 65,536 values, and the one inequality -- `> 0x53` at
 * 0x62acd -- only decides whether to test equality with 0x54, which is false
 * either way.  Recorded as an equivalent mutation so the claim is filed
 * rather than assumed.
 */
static void
txblock_tail(struct v34_object *obj, short txstate)
{
	struct v34_receiver *rx =
	    (struct v34_receiver *)((char *)obj + TB_RECEIVER);
	int sel = TB_INT(obj, TB_F2218);

	if (sel == 1) {
		/*
		 * 0x62b45.  Three times the halfword at +0xaa96 into the
		 * receiver's +0x1d2, and then on into the rest of the tail --
		 * this is a branch out and back, not an arm of its own.
		 */
		rx->f1d2 = (short)(3 * (int)obj->faa96);
		obj->f0004 = 4;
	}

	/* 0x62a56 */
	if ((unsigned)(sel - 4) <= 1u)
		obj->f0004 = 6;

	/* 0x62a68, and 0x64884 when both halves hold */
	if ((unsigned)(sel - 2) <= 1u && txstate == 0x4a)
		obj->f0004 = obj->vect_idx > 0x3c ? 0 : 7;

	/* 0x62a7a.  UNSIGNED: `cmp %ebx,0x234(%esi)` then `jbe`. */
	if ((unsigned)TB_INT(obj, TB_F0238) > (unsigned)TB_INT(obj, TB_F023C))
		obj->f0004 = 8;

	/*
	 * 0x62a92.  SIGNED, and the short is sign-extended before the
	 * comparison: `movswl 0x134(%edi),%esi` then `cmp` and `jge`.
	 * At or above the floor the counter is cleared (0x62b07); below it,
	 * it advances.
	 */
	if ((int)rx->agc_level >= obj->rx_energy_floor)
		TB_INT(obj, TB_F0234) = 0;
	else
		TB_INT(obj, TB_F0234) += 1;

	/* 0x62aaf.  SIGNED: `cmpl $0x257f` then `jle`. */
	if (TB_INT(obj, TB_F0234) > 0x257f)
		obj->f0004 = 9;

	/* 0x62ac5.  Three states above the table's own range. */
	if (txstate == 0x53)
		obj->f0004 = 0xf;		/* 0x62b2f  MOH_FRR        */
	else if (txstate > 0x53) {
		if (txstate == 0x54)
			obj->f0004 = 0x10;	/* 0x62b1a  MOH_CLEARDOWN  */
	} else if (txstate == 0x52)
		obj->f0004 = 0xd;		/* 0x64a4f  MOH_ON_HOLD    */
}

/*
 * 0x64480, txstate 5 SILENCE -- the only arm that reads another machine.
 *
 * It branches out three times, to 0x6778b on the microstate, and to 0x655c9
 * and 0x67d48 on the rxstate; all three come back here or set +0x0004 to 1.
 * The microstate branch is taken FIRST and returns to the rxstate tests when
 * +0x359c is not 0x66, so reaching 0x67d48's own body needs microstate 63 and
 * rxstate 35 together with +0x359c == 0x65 -- a round trip out and back.
 * Finding 363.
 */
static void
txblock_silence(struct v34_object *obj)
{
	unsigned short mst = TB_USHORT(obj, TB_MICROSTATE);
	unsigned short rxst;

	/* 0x6448e, and 0x6778b when it holds */
	if (mst == 0x3f && obj->f359c == 0x66) {
		obj->f0004 = 1;			/* 0x67799 */
		return;
	}

	/* 0x64498 */
	rxst = TB_USHORT(obj, TB_RXSTATE);
	if (rxst == 4) {
		/* 0x655c9 */
		if (mst == 0x2c && obj->f359c == 0x65) {
			obj->f0004 = 1;
			return;
		}
	} else if (rxst == 0x23) {
		/* 0x67d48, which rejoins 0x67799 */
		if (mst == 0x3f && obj->f359c == 0x65) {
			obj->f0004 = 1;
			return;
		}
	}

	obj->f0004 = 0;				/* 0x644ba */
}

/*
 * 0x64518, txstates 18 SSEG and 19 SBARSEG.
 *
 * Both answers are 2 or 3, and 0x6780f -- the branch out on +0x359c -- gives
 * 2, which is also what a clear bit gives.  So the branch is not observable
 * in +0x0004 alone; what separates the two paths is that one reads the
 * receiver's flags word and the other does not.
 */
static void
txblock_sseg(struct v34_object *obj)
{
	const struct v34_receiver *rx =
	    (const struct v34_receiver *)((char *)obj + TB_RECEIVER);

	if (obj->f359c == 0x65) {
		obj->f0004 = 2;			/* 0x6780f */
		return;
	}
	/* 0x64531: bit 3 of the receiver's flags at +0x122. */
	obj->f0004 = (rx->flags >> 3) & 1 ? 3 : 2;
}

void
v34handshak_txblock(struct v34_object *obj)
{
	short txstate = (short)TB_USHORT(obj, TB_TXSTATE);

	/*
	 * 0x62af1.  The object tests `(unsigned)(txstate - 5) <= 0x45` and
	 * sends everything else to the default arm, which is the same block
	 * the table's own fifty-four default entries name.  A switch over the
	 * state values is the same partition of the same domain: the bound is
	 * not separately observable, because both sides of it reach 0x62a40
	 * with the same txstate.
	 */
	switch (txstate) {
	case 5:					/* 0x64480 */
		txblock_silence(obj);
		break;
	case 18: case 19:			/* 0x64518 */
		txblock_sseg(obj);
		break;
	case 20: case 21: case 64: case 68:	/* 0x64509 */
		obj->f0004 = 2;
		break;
	case 24: case 51: case 54: case 60: case 74:	/* 0x644c9 */
		obj->f0004 = 0;
		break;
	case 66: case 67: case 69:		/* 0x644fa */
		obj->f0004 = 3;
		break;
	case 70:				/* 0x644d8 */
		/*
		 * `cmp $0x1,%bp` then `sbb %eax,%eax` and `add $0x4,%eax`:
		 * the borrow is set only when the halfword is zero.
		 */
		obj->f0004 = TB_USHORT(obj, TB_F0E4C) == 0 ? 3 : 4;
		break;
	default:				/* 0x62a40, straight to the tail */
		break;
	}

	txblock_tail(obj, txstate);
}
