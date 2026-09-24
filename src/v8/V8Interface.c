/*
 * V8Interface.c -- V.8's public interface.
 *
 * The blob's first V.8 translation unit.  It allocates and frees the state
 * (`V8Create`/`V8Delete`), builds and reads the negotiated messages
 * (`V8SetMessage`/`V8GetMessage`/`V8UpdateModemParameters`), arms the
 * out-of-band request handler (`V8Control`) and runs the per-sample loop
 * (`V8Process`).  The sequence builders initTxSequence, rebuildJMSequence
 * and evaluateRxJMSequence are V8.c's, as the object's FILE and address
 * order show; `ext_word`, the one static both units need, is in v8int.h.
 */

#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/v8.h"
#include "dsplib/sysdep.h"
#include "v8int.h"

/* ---- V8Create, V8Delete  (v8hs.c) ---- */
/*
 * Build a handshake.
 *
 * Note what is NOT here: the object is allocated and never zeroed.  Only the
 * six configuration words below and whatever `v8handshakinit` writes are
 * defined when this returns, and the rest is whatever the allocator had.  A
 * caller that reads anything else is reading rubbish -- which is worth
 * knowing, because on a fresh page that rubbish is usually zero and so looks
 * deliberate.
 *
 * THE GUARD IS SINGLE-EXIT AND THAT IS READ OFF THE OBJECT, not a style
 * choice.  The blob's `je` lands on `add $0x34,%esp` with the `mov %esi,%eax`
 * BELOW it, so the failure path falls through the same return the success
 * path uses.  Written as an early `return`, GCC 3.4.2 has a second value to
 * materialise, spends `xor %eax,%eax` on it before the first `cfg` copy --
 * which costs `%eax` as a store base for two of them -- and has to jump PAST
 * the shared `mov`.  All three early-return spellings (`return 0`, `return v`,
 * `if (!v) return v`) compile to ONE emission, because the compiler knows `v`
 * is null on that arm and the returned expression is free; both single-exit
 * spellings compile to another.  A two-element domain, exhausted, and only
 * one element produces the object's control flow: 167 differing bytes to 25.
 *
 * The rate copy uses byte access to retain the dependency between its store
 * and the following CM pointer load.  A scalar int assignment lets GCC
 * 3.4.2 hoist that pointer load, changing 25 bytes; memcpy reproduces all
 * 1124 bytes.  The six field values and their source order are unchanged.
 * See F10218 for the finite domain and the compiler's alias-set evidence.
 */
struct v8 *
V8Create(const struct v8_cfg *cfg)
{
	struct v8 *v = sysdep_malloc(sizeof(struct v8));

	if (v != 0) {
		v->side = cfg->side;
		v->op_mode = cfg->op_mode;
		v->timeout_a = cfg->timeout_a;
		v->timeout_b = cfg->timeout_b;
		memcpy(&v->rate, &cfg->rate, sizeof(v->rate));
		v->cm = cfg->cm;

		/*
		 * The configuration trace: seventeen messages, each behind its own
		 * gate.  This is where the author dates the module (23/09/03) and
		 * names what the fields mean -- `side` and `op_mode` were called
		 * operation mode, `offered` the ansPcmLevel, `menu` the ucodeForQts,
		 * and the two CM extension fields are raw call-function and protocol
		 * octets.  See finding F164.
		 *
		 * Every message ends \r\n -- all of V8's diagnostics do.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: Create called, V8 version 23/09/03 .\r\n");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("#################################"
					     "###########################\r\n");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V8: local configuration : \r\n");
		/*
		 * `== 0` with "Caller" first, and both halves of that are the
		 * object's.  The blob branches `je` where we branched `jne` --
		 * one byte, 0x74 against 0x75, same displacement, same target
		 * -- and its `.rodata.str1.1` holds "Caller" before "Answer"
		 * where ours held them the other way round.  GCC interns
		 * literals in source-text order, so the pool order is a second
		 * observable that agrees without reference to any instruction
		 * (lever 2).  Four spellings compiled; the two that put
		 * "Caller" first both reach it, so what is decoded is the arm
		 * ORDER and not `== 0` against `!v->side`.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tSide = %s\r\n",
					     v->side == 0 ? "Caller" : "Answer");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tOperation Mode = %d\r\n", v->op_mode);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "\tModulations - V90=%d, V34=%d, V34HD=%d, V32=%d, "
			    "V22=%d, V17=%d, V29=%d, V27=%d, V23=%d, V21=%d\r\n",
			    (v->cm->b0 >> 3) & 1, (v->cm->b0 >> 5) & 1,
			    (v->cm->b0 >> 6) & 1, v->cm->b0 >> 7,
			    v->cm->b1 & 1, (v->cm->b1 >> 1) & 1,
			    (v->cm->b1 >> 2) & 1, (v->cm->b1 >> 3) & 1,
			    (v->cm->b1 >> 4) & 1, (v->cm->b1 >> 5) & 1);

		/* The presence bits are tested outside the gates, not inside. */
		if (v->cm->b2 & V8_CM_EXT1_PRESENT) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "\tCall Functions - raw CF specified: cf[0]=%d , "
				    "cf[1]=%d , cf[2]=%d , cf[3]=%d\r\n",
				    v->cm->ext1[0], v->cm->ext1[1],
				    v->cm->ext1[2], v->cm->ext1[3]);
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(
			    "\tCall Functions - Data=%d, CallRxFax=%d, CallTxFax=%d, "
			    "V.80=%d\r\n",
			    (v->cm->b1 >> 6) & 1, v->cm->b1 >> 7,
			    v->cm->b2 & 1, (v->cm->b2 >> 1) & 1);
		}

		if (v->cm->b2 & V8_CM_EXT2_PRESENT) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "\tProtocol - raw Protocol specified: prot[0]=%d "
				    ", prot[1]=%d , prot[2]=%d , prot[3]=%d\r\n",
				    v->cm->ext2[0], v->cm->ext2[1],
				    v->cm->ext2[2], v->cm->ext2[3]);
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf("\tProtocol - LAPM V.42\r\n");
		}

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tv8bisIndication - %d\r\n",
					     (v->cm->b0 >> 1) & 1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\ttimeouts - signal detect %d sec, "
					     "message detect %d sec\r\n",
					     v->timeout_a, v->timeout_b);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tquickConnectEnabled - %d\r\n",
					     (v->cm->b2 >> 4) & 1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tlapmIndication - %d\r\n",
					     (v->cm->b2 >> 6) & 1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tucodeForQts - %d\r\n", v->cm->menu);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tansPcmLevel - %d\r\n", v->cm->offered);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("#################################"
					     "###########################\r\n");

		v->tx_gain = 0x4000;
		v8handshakinit(v);

		v->pole_state = 0;
		v->prev_status = 0;
	}
	return v;
}

void
V8Delete(struct v8 *v)
{
	if (v != 0)
		sysdep_free(v);
}

/* ---- v8StatusName / v8ControlName / v8SequenceName  (v8seq.c) ---- */
/*
 * .rodata+0x53ac.  A GLOBAL symbol in the object -- the only exported data
 * in all of V.8 -- so it is one here too.  V8SetMessage indexes it with the
 * selector for its entry banner.
 */
/*
 * And the status names, .rodata+0x53c0 -- also global, nineteen entries, and
 * indexed directly by what V8Process returns.  The seventh is thirty-two
 * characters and stops mid-word; that is what is in the object, so that is
 * what is here.
 */
const char *const v8StatusName[V8_LAST_ENUM + 1] = {
	"V8_INIT",
	"V8_ANS_SEND_ANSAM",
	"V8_ANS_CM_DETECTED",
	"V8_ANS_SEND_JM",
	"V8_ANS_TIME_OUT_WAITING_FOR_CM",
	"V8_ANS_TIME_OUT_WAITING_FOR_CJ",
	"V8_ORG_WAITING_FOR_ANSAM",
	"V8_ORG_ANSAM_DETECTED_WAITING_TE",
	"V8_ORG_SEND_CM",
	"V8_ORG_JM_DETECTED",
	"V8_ORG_SEND_CJ",
	"V8_ORG_TIME_OUT_WAITING_FOR_ANSAM",
	"V8_ORG_TIME_OUT_WAITING_FOR_JM",
	"V8_OK",
	"V8_ORG_SEND_QC",
	"V8_ORG_WAITING_FOR_QCA1d",
	"V8_ORG_BAD_QCA1d_MESSAGE",
	"V8_ORG_TIME_OUT_WAITING_FOR_QCA1d",
	"V8_LAST_ENUM"
};

/*
 * And the control-request names, .rodata+0x5380.  Eleven entries, of which
 * the last eight are the original's own placeholders.
 */
const char *const v8ControlName[V8CTRL_LAST + 1] = {
	"V8CTRL_START_CM",
	"V8CTRL_START_CJ",
	"V8CTRL_START_JM",
	"V8CTRL_CTRL3",
	"V8CTRL_CTRL4",
	"V8CTRL_CTRL5",
	"V8CTRL_CTRL6",
	"V8CTRL_CTRL7",
	"V8CTRL_CTRL8",
	"V8CTRL_CTRL9",
	"V8CTRL_CTRL10"
};

const char *const v8SequenceName[4] = {
	"V8_CM", "V8_JM", "V8_CJ", "V8_QC1A"
};

/* ---- selected_sequence  (v8seq.c) ---- */
/*
 * Which buffer a selector names.  The order is not the order they sit in
 * memory: selector 1 is the third buffer and selector 2 the second.  Kept as
 * the original has it -- guessing that the swap is a mistake and "fixing" it
 * would put messages in the wrong place.
 */
static struct v8_tx_sequence *
selected_sequence(struct v8 *v, int which)
{
	switch (which) {
	case V8_SET_CM:	return &v->seq[0];
	case V8_SET_JM:	return &v->seq[2];
	case V8_SET_CJ:	return &v->seq[1];
	case V8_SET_QC1A: return &v->seq[3];
	default:	return 0;
	}
}

/* ---- V8SetMessage  (v8seq.c) ---- */
int
V8SetMessage(struct v8 *v, int which, const unsigned char *octets, int n)
{
	struct v8_tx_sequence *seq = selected_sequence(v, which);
	int rc = 0;
	int i;

	/*
	 * Note the line endings: the entry banner is \r\n like the rest of
	 * V8's trace, but all three complaints below end in a bare \n.
	 */
	if (seq == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V8: Try to set message with "
					     "illegal V.8 message type\n");
		return V8_SET_REJECTED;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V8: V8SetMessage called, message type is %s\r\n",
		    v8SequenceName[which]);

	if (n == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: Try to set message with 0 length\n");
		return V8_SET_REJECTED;
	}

	if (n > V8_TX_SEQ_WORDS) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: Try to set message with size larger than "
			    "maximal (max=%d, externalLength=%d)\n",
			    V8_TX_SEQ_WORDS, n);
		n = V8_TX_SEQ_WORDS;
		rc = V8_SET_TRUNCATED;
	}

	for (i = 0; i < n; i++)
		seq->word[i] = ext_word(octets[i]);

	seq->crc = (short)0xffff;
	seq->bitpos = 0;
	seq->wordidx = 0;
	seq->repeats = 0;
	seq->nbits = (short)(n * V8_SEQ_BITS_PER_WORD);
	seq->wordbits = V8_SEQ_BITS_PER_WORD;
	seq->crc_enable = 0;
	seq->shifter = 0;
	seq->shifter0 = 0;
	seq->nleft = 0;
	seq->nleft0 = 0;
	seq->repeat = 1;

	return rc;
}

/* ---- V8Control  (v8sig.c) ---- */
/*
 * Nudge the handshake from outside.
 *
 * Each request is accepted only from the one state it makes sense in, and
 * refused otherwise -- there is no queueing and no error beyond the return
 * value, so a caller that asks at the wrong moment simply gets -1.
 */
int
V8Control(struct v8 *v, int what)
{
	int rc;

	switch (what) {
	case V8CTRL_START_CM:
		if (v->side != 0 || v->rx_state != 0x19 || v->cm_ready != 0) {
			rc = -1;
		} else {
			v->cm_ready = 1;
			rc = 0;
		}
		break;

	case V8CTRL_START_CJ:
		if (v->rx_substate != V8_HS_TAKEN_RX) {
			rc = -1;
		} else {
			v->rx_substate = V8_HS_DRAIN;
			rc = 0;
		}
		break;

	case V8CTRL_START_JM:
		if (v->rx_substate != V8_HS_TAKEN_TX) {
			rc = -1;
		} else {
			v->rx_substate = V8_HS_CJ;
			v->elapsed = 0;
			v->tx_state = 0x17;
			rc = 0;
		}
		break;

	default:
		/*
		 * The only exit that does not name the request: there is no
		 * name to give it, so it reports the number instead -- and it
		 * is the one message here that ends \r\n.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: V8Control called with currently not "
			    "supported control type (type=%d)\r\n", what);
		return -1;
	}

	/*
	 * Announced whether it was accepted or refused -- the object has two
	 * copies of this, one per return value, sharing the one call.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V8: V8Control called - control type is %s\n",
		    v8ControlName[what]);
	return rc;
}

/* ---- V8Process  (v8proc.c) ---- */
/* The receive filter's one pole, in Q12. */
#define V8_RX_POLE	0xf85

int
V8Process(struct v8 *v, const short *in, short *out, int count)
{
	int status = 0;
	int changed = 0;
	int i;

	for (i = 0; i < count; i++) {
		int x;
		int c;

		/* One sample out of the transmit ring. */
		v->tx_avail = (short)(v->tx_avail - 1);
		*out++ = *v->tx_ring_base++;
		if (v->tx_ring_base >= v->tx_ring + V8_TX_RING_END)
			v->tx_ring_base = v->tx_ring;

		/*
		 * And one in, through a single pole, into the symbol buffer.
		 * The imaginary half is written as zero: what arrives is
		 * real, and the demodulator expects pairs.
		 */
		x = *in++;
		c = (short)(x + (unsigned short)v->pole_state);
		v->pole_state = (short)((c * V8_RX_POLE - (x << 12)) >> 12);
		v->tx_sym_b[0] = (short)c;
		v->tx_sym_b[1] = 0;
		v->tx_sym_b += 2;
		if (v->tx_sym_b >= v->tx_symbols + V8_TX_SYMBOLS)
			v->tx_sym_b = v->tx_symbols;
		v->sym_avail = (short)(v->sym_avail + 1);

		/* Run the machine when there is room to send or work to do. */
		if ((short)v->tx_avail <= 4 || (short)v->sym_avail > 4) {
			if ((short)v8handshak(v) == 2)
				changed = 1;
		}
	}

	/*
	 * The status.  Which state variable decides depends on the shape of
	 * handshake, and in the answering shape the receive state can
	 * overwrite what the transmit state chose.
	 */
	if (v->side == 0) {
		if (v->tx_state == 5 && v->rx_state == 0x19 && v->rx_substate == 0x19)
			status = V8_ORG_WAITING_FOR_ANSAM;
		else if ((unsigned short)v->rx_substate == 0x24)
			status = V8_ORG_ANSAM_DETECTED_WAITING_TE;
		else if ((unsigned short)v->tx_state == 0x17)
			status = v->tx_seq == &v->seq[1]
				 ? V8_ORG_SEND_CJ
				 : V8_ORG_SEND_CM + (v->rx_substate == V8_HS_TAKEN_RX);
		else if ((unsigned short)v->tx_state == 0x2b)
			status = V8_ORG_SEND_QC;
		else if ((unsigned short)v->rx_state == 0xb)
			status = V8_ORG_TIME_OUT_WAITING_FOR_ANSAM;
		else if ((unsigned short)v->rx_state == 0xc)
			status = V8_ORG_TIME_OUT_WAITING_FOR_JM;
	} else {
		if ((unsigned short)v->tx_state == 6)
			status = V8_ANS_SEND_ANSAM
				 + (v->rx_substate == V8_HS_TAKEN_TX);
		else if ((unsigned short)v->tx_state == 0x17)
			status = V8_ANS_SEND_JM;

		if ((unsigned short)v->rx_state == 4)
			status = V8_ANS_TIME_OUT_WAITING_FOR_CM;
		else if ((unsigned short)v->rx_state == 5)
			status = V8_ANS_TIME_OUT_WAITING_FOR_CJ;
	}

	if (changed)
		status = V8_OK;

	/*
	 * The conditional store is a change detector: this is the one place
	 * the whole negotiation is narrated, and `feb8` exists to hold the
	 * previous status so that it can be.
	 */
	if (v->prev_status != status) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: State changed from %s to %s\r\n",
			    v8StatusName[v->prev_status], v8StatusName[status]);
		v->prev_status = status;
	}
	return status;
}

/* ---- V8UpdateModemParameters  (v8jm.c) ---- */
/*
 * Turn a received sequence into a call menu.
 *
 * The words carry their meaning in their top nibble: 0x14 opens the
 * modulation list, 0x16 and 0x1c carry two more flags, and a word whose bit 4
 * is set says another word of the same group follows.  Everything the menu
 * does NOT offer is cleared, so the result is the intersection of what was
 * asked for and what came back.
 */
int
V8UpdateModemParameters(struct v8 *v, struct v8_cm *out)
{
	struct v8_tx_sequence *seq = v->side != 0 ? &v->seq[0] : &v->seq[2];
	int v90_mod = 0, digital_connection = 0, pcm_indication = 0;
	int i;

	out->b2 = (unsigned char)((out->b2 & 0xef)
				  | (((unsigned char)v->quick_connect & 1) << 4));
	out->offered = v->anspcm_level;
	out->b2 = (unsigned char)((out->b2 & 0xbf)
				  | ((v->lapm_indication != 0 && (out->b2 & 0x40)) << 6));

	if (v->quick_connect != 0) {
		/*
		 * `quick_connect` set means the quick-connect sequence was
		 * used, and this is where the author says so -- which is how
		 * the field gets its meaning.  Announced before the menu is
		 * stamped.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8Report: Finished with Quick Connect\n");
		out->b0 |= 9;
		return 0;
	}

	if ((short)seq->wordidx <= 0)
		return -1;

	out->b2 &= 0xf8;
	out->ext1[0] = 0;
	out->b1 &= 0x3f;

	if (v->fn_matched != 0) {
		short fn = v->fn_word;

		if (fn == 0x107) {
			out->b1 |= 0x40;
		} else if (fn == 0x109) {
			out->b2 |= 2;
		} else if (fn == 0x10b) {
			out->b1 |= 0x80;
		} else if (fn == 0x103) {
			out->b2 |= 1;
		} else if (fn == 0) {
			/*
			 * `fn_matched` said a function matched but nothing was
			 * remembered.  The complaint is all that happens: the
			 * walk below still runs, so the modulation list is
			 * still intersected.
			 */
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V8Report: Didn't get " "matching call "
						     "function\r\n");
		} else {
			/* Something else: keep it as an extension octet. */
			out->b2 |= 4;
			out->ext1[0] = charFlip((unsigned char)(fn >> 1));
		}

		/* Walk the sequence for the modulation list and its flags. */
		for (i = 0; i < (short)(unsigned short)seq->wordidx; i++) {
			unsigned w = (unsigned short)seq->word[i];
			unsigned top = w >> 4;

			if (top == 0x14) {
				unsigned f = (w & 0xf) >> 1;

				if ((f & 2) == 0)
					out->b0 &= 0xdf;
				if ((f & 1) == 0)
					out->b0 &= 0xbf;
				v90_mod = (int)(f >> 2);

				if (((unsigned short)seq->word[i + 1] & 0x10)
				    == 0)
					continue;
				i++;
				f = (unsigned short)seq->word[i] >> 1;
				if ((f & 0x01) == 0)
					out->b1 &= 0xf7;
				if ((f & 0x02) == 0)
					out->b1 &= 0xfb;
				if ((f & 0x20) == 0)
					out->b1 &= 0xfd;
				if ((f & 0x40) == 0)
					out->b1 &= 0xfe;
				if ((f & 0x80) == 0)
					out->b0 &= 0x7f;

				if (((unsigned short)seq->word[i + 1] & 0x10)
				    == 0)
					continue;
				i++;
				f = (unsigned short)seq->word[i];
				if ((f & 0x40) == 0)
					out->b1 &= 0xef;
				if ((f & 3) == 0)
					out->b1 &= 0xdf;
			} else if (top == 0x16) {
				digital_connection = (int)((w >> 1) & 1);
			} else if (top == 0x1c) {
				pcm_indication = (int)((w >> 2) & 3);
			}
		}

		/*
		 * What the three gathered words mean.  The names are the
		 * author's, from the line below: modulation, digital
		 * connection, PCM indication -- V.90's three preconditions.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8Report: remote V90: mod - %d, digital "
			    "connection - %d, pcmIndication - %d\n",
			    v90_mod, digital_connection, pcm_indication);

		/*
		 * The three flags together decide one bit of the menu: the
		 * offer only stands if all of them agree.
		 */
		out->b0 = (unsigned char)((out->b0 & 0xf7)
			  | (((out->b0 & 8) && v90_mod != 0
			      && digital_connection != 0
			      && pcm_indication == 1) << 3));
	} else if (DSPLIB_DEBUG_ON()) {
		dsplibs_debug_printf("V8Report: since no call function match, "
				     "not indicating any modulation " "capability...\r\n");
	}

	/* The second extension, if one came back that is not the filler. */
	if (v->ext2_word != 0 && v->ext2_word != 0xa9) {
		out->b2 |= 8;
		out->ext2[0] = charFlip((unsigned char)(v->ext2_word >> 1));
	}

	out->b0 |= 1;

	/*
	 * The menu as it now stands, bit for bit -- the same ten bits, in the
	 * same order, that V8Create prints for the local configuration.  The
	 * two lines side by side are what the negotiation asked for and what
	 * it settled on.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V8Report: v90:%d, v34:%d, v34hd:%d, V32:%d, V22:%d, "
		    "V17:%d, V29:%d, V27:%d, V23:%d, V21:%d\n",
		    (out->b0 >> 3) & 1, (out->b0 >> 5) & 1,
		    (out->b0 >> 6) & 1, out->b0 >> 7,
		    out->b1 & 1, (out->b1 >> 1) & 1, (out->b1 >> 2) & 1,
		    (out->b1 >> 3) & 1, (out->b1 >> 4) & 1,
		    (out->b1 >> 5) & 1);

	return 0;
}

/* ---- rx_sequence  (v8seq.c) ---- */
/*
 * Which of the five buffers holds what was received.  Three cases, and the
 * middle one is the reason the object keeps a spare pointer at all: once
 * `quick_connect` is set the handshake has moved on and the message lives
 * wherever that pointer says, rather than at a fixed place.
 */
static const struct v8_tx_sequence *
rx_sequence(const struct v8 *v)
{
	if (v->quick_connect != 0)
		return v->seq_spare;
	if (v->side != 0)
		return &v->seq[0];
	return &v->seq[2];
}

/* ---- V8GetMessage  (v8seq.c) ---- */
int
V8GetMessage(struct v8 *v, unsigned char *out, int *count)
{
	const struct v8_tx_sequence *seq = rx_sequence(v);
	int n = seq->wordidx;
	int rc = 0;
	int i;

	if (n <= 0)
		return V8_GET_EMPTY;

	/*
	 * Too long for the caller's buffer: fill what fits and hand back the
	 * length it would have needed, which is how truncation is told apart
	 * from a message that was simply this short.
	 */
	if (n > *count) {
		rc = n;
		n = *count;
	}

	for (i = 0; i < n; i++)
		out[i] = charFlip((unsigned char)(seq->word[i] >> 1));

	*count = n;
	return rc;
}
