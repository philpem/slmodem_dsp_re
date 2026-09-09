/*
 * voice.c -- the voice service's TU (blob spans `voice.c#1..#3`).  What is
 * reconstructed so far is the SOFTWARE RING DETECTOR, which is all nine
 * symbols of it, in the object's own emission order:
 *
 *     0x2130 RD_create           0x2380 RingDetector_Reset
 *     0x2260 RD_delete           0x2550 RingDetector_Create
 *     0x22c0 RD_process          0x2720 RingDetector_GetLastRing
 *     0x2350 RD_ring_details     0x2740 RingDetector_Process
 *     0x2360 RingDetector_Delete
 *
 * ...and, at the END of the file, the other eight of the span's symbols, in
 * the object's own order among themselves:
 *
 *     0x0600 vce_hook_on        0x0910 VOICE_delete
 *     0x0630 vce_hook_off       0x09a0 VOICE_command
 *     0x0660 vce_get_sreg       0x0bd0 VOICE_process
 *     0x0720 VOICE_create       0x13b0 STRM_VCE_GetFDSPEnvironmentalParams
 *
 * THAT GROUP IS OUT OF EMISSION ORDER RELATIVE TO THE RING DETECTOR, on
 * purpose, and that is a deviation from this tree's usual rule, so it is
 * written down rather than left to be discovered.  All eight precede
 * `RD_create` in the object -- the first three are the span's first three
 * symbols -- so faithful order would put the whole group at the top of this
 * file.  It is appended instead because emission order is a register-
 * allocation carrier (CLAUDE.md's lever 2), the ring detector above was
 * measured in its current position, and neither the session that wrote the
 * `vce_*` half nor the one that wrote the `VOICE_*` half had a period
 * compiler with which to re-measure it after a move.  The eight are at least
 * in the right order among THEMSELVES, which is all that can be settled
 * without the measurement.  Whoever next runs `byteident.py` over this file
 * should try the faithful order and keep it if nothing above regresses.
 * Findings F8773 and F8835.
 *
 * FOUR MORE FOLLOW `STRM_VCE_GetFDSPEnvironmentalParams` IN THE OBJECT,
 * `voice.c#3 +3`'s own symbols despite the `FAX_` names -- this is the fax
 * SERVICE dispatcher's own entry points, not the Class 1 fax machine
 * (`class1.c`/`class1rx.c`/`class1tx.c`), which they call into:
 *
 *     0x1450 FAX_delete          0x1740 FAX_class1_command (NOT written)
 *     0x1500 FAX_create (NOT written)
 *     0x1a10 FAX_process
 *
 * `FAX_delete` and `FAX_process` are written -- `FAX_create` and
 * `FAX_class1_command` are blocked, RE-VERIFIED this wave (F10111):
 * `FAXVMI_create` has since landed and is no longer the reason, but
 * `FAXVMI_control` (`faxvmi.c`) is still unwritten and still the shared
 * chokepoint, alongside four `class1tx.c` leaf inits and
 * `_init_receiver`/`_init_transmitter` (`class1rx.c`/`class1tx.c`), which
 * are themselves blocked on the same `FAXVMI_control`.  See `fax.h` for
 * `struct fax_ctx`, which is NOT `struct voice_ctx` despite sharing this TU,
 * and docs/findings.md F10105/F10111.
 *
 * The TU is otherwise complete for voice: nothing in the `voice.c#1..#3`
 * spans but these three is unwritten.
 *
 * WHAT THE DETECTOR IS.  A hysteretic zero-crossing counter run over the
 * incoming 16-bit samples.  `RD_create` picks a threshold from the codec type
 * and hands `RingDetector_Create` a configuration block; `RingDetector_Reset`
 * clamps it and derives three working values from it; `RingDetector_Process`
 * runs a three-state comparator per sample, measures the frequency once per
 * cycle, averages it, and declares a ring once the measured tone has lasted
 * `minOnDur` milliseconds.  It returns 1 on the sample where its verdict
 * CHANGES, and slmodemd then asks `RD_ring_details` for the frequency and
 * duration -- a frequency of 0 meaning "ring starting" and a positive one
 * meaning "ring finishing" (`modem.c`, `modem_ring_detector_process`).
 *
 * TWO FACTORING NOTES, so a per-function byte count is read correctly.
 *
 *   - `RingDetector_Create` in the object is `malloc` followed by the WHOLE
 *     BODY of `RingDetector_Reset`, which GCC 3.4 at -O3 inlines while still
 *     emitting the out-of-line copy the rest of the object calls.  It is
 *     written here as the call it must have been.
 *   - `rd_guard` and `rd_measure` are static helpers with no symbol in the
 *     blob: the object holds one shared copy of the guard-band arm (reached
 *     from both half cycles by cross-jumping) and two identical copies of the
 *     measurement.  Their bytes count against neither side -- CLAUDE.md's
 *     inlining-boundary trap.
 */

#include "dsplib/ringdet.h"
#include "dsplib/vce.h"
#include "dsplib/class1.h"
#include "dsplib/debug.h"
#include "dsplib/fax.h"
#include "dsplib/fixedrc.h"
#include "dsplib/sysdep.h"
#include "dsplib/modem_params.h"
#include "dsplib/voice.h"

/*
 * `struct vce_ring` holds no pointers, so its 0x310 bytes are the same claim
 * off ILP32 and can be asserted outright.  `struct vce` holds five, so
 * `sizeof == 0x1484` is NOT portable and is not written: what is asserted here
 * is the pointer-free part of the layout -- the span from the first buffer to
 * the first ring, and the gap between the two rings -- and the literal 0x1484
 * is held by t_voiceapi, which compares `harness_alloc_reqsize` on the 32-bit
 * differential build.  That is the same division `struct rd` above makes, and
 * for the same reason: a `__SIZEOF_POINTER__` guard would read `#if 0` under
 * the period compiler and delete the assertion silently
 * (docs/method/compilers.md).
 */
typedef char vce_ring_size_check[sizeof(struct vce_ring) == 0x310 ? 1 : -1];
typedef char vce_buffers_check[
    offsetof(struct vce, out_ring) - offsetof(struct vce, host_in)
	== 0x0e64 - 0x0020 ? 1 : -1];
typedef char vce_rings_check[
    offsetof(struct vce, in_ring) - offsetof(struct vce, out_ring)
	== 0x310 ? 1 : -1];

/*
 * The host's two byte pipes.  They are slmodemd's, not this object's, and
 * they are declared here for the same reason src/service/cid.c declares
 * `modem_send_to_tty` locally: there is no dsplib header that owns them.
 * Both signatures are slmodemd's own (`modem.c`, `modem_read`/`modem_write`
 * paths), and the object agrees -- each is called with three arguments and
 * each result is used as a 32-bit signed count.
 */
extern int modem_recv_from_tty(void *m, void *buf, int n);
extern int modem_send_to_tty(void *m, const void *buf, int n);

typedef char ring_detector_size_check[
    sizeof(struct ring_detector) == 0x54 ? 1 : -1];
/*
 * `struct rd` is 8 bytes in the object and two POINTERS wide, which is not
 * the same claim off ILP32 -- so the portable half is asserted here and the
 * literal 8 is held by t_ringdet, which compares `harness_alloc_reqsize` on
 * the 32-bit differential build.  A `__SIZEOF_POINTER__` guard would not do:
 * that predefine is GCC 4.6+, so under the period compiler the guard reads
 * `#if 0` and the assertion silently disappears (docs/method/compilers.md).
 */
typedef char rd_size_check[
    sizeof(struct rd) == 2 * sizeof(void *) ? 1 : -1];

/*
 * The two sample rates the detector is written for.  `RD_create` refuses
 * anything else before it allocates.
 */
#define RD_RATE_8000	8000
#define RD_RATE_9600	9600

/*
 * The comparator threshold, chosen from `MDMPRM_CODECTYPE`.  The object
 * names no codec constant anywhere -- the mangling of the V.90 code records
 * only that `__tHardwareCodecTypes__` exists, not its enumerators (see
 * V90CodecType.h) -- so these stay as the numbers the switch tests.
 *
 * The default is NEGATIVE, and that is not a sentinel: Reset takes the
 * threshold's absolute value everywhere and uses its SIGN to pick the second
 * set of debounce constants (0/200 rather than 2/100).
 */
#define RD_THRESHOLD_DEFAULT	(-3000)

/* ------------------------------------------------------------------ *
 * The `vce_*` / `STRM_VCE_*` group.  See the note at the top of the   *
 * file about why these sit here and not before `RD_create`.           *
 * ------------------------------------------------------------------ */

/*
 * Off-hook and on-hook notifications.  Both are pure diagnostics in this
 * object: the whole body is the `> 1` gate and one printf, and the argument
 * is printed with `%p` and otherwise untouched, so nothing observable happens
 * at level 0 or 1.  Kept for the reason debug.h gives -- the call site is the
 * author's annotation, and a reconstruction that dropped it would differ in
 * control flow from the object even where the output agreed.
 */
static void
vce_hook_on(void *p)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("voice: vce_hook_on (%p)...\n", p);
}

static void
vce_hook_off(void *p)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("voice: vce_hook_off (%p)...\n", p);
}

/*
 * The voice service's own S-register reader.
 *
 * slmodemd has a `modem_get_sreg`, and this is NOT a call into it: the object
 * fetches `struct voice_info` under MDMPRM_VOICEINFO and answers seven
 * register numbers out of that block and out of three built-in constants.
 * Everything else reads back 0, including every register the host would have
 * had a value for.
 *
 * The seven, and where each answer comes from:
 *
 *   S24  flash timer                     20, constant
 *   S72  handset gain                    19, constant
 *   S73  voice dial-tone detect delay     3, constant (seconds)
 *   S82  #VSS silence sensitivity        voice_info.silence_detect_sensitivity,
 *                                        reduced to a 0..3 level
 *   S83  #VSP silence period             voice_info.silence_detect_period
 *   S138 mic gain                        voice_info.rx_gain
 *   S139 line record gain                voice_info.rx_gain
 *
 * The block is fetched BEFORE the switch, unconditionally, so the three
 * constant answers still cost a `modem_get_param` call -- which is visible in
 * the object (the call is the first thing the function does) and is worth
 * preserving because a caller's parameter log can see it.
 */
static int
vce_get_sreg(void *modem, unsigned int num)
{
	struct voice_info *vi;
	unsigned int level;

	vi = (struct voice_info *)modem_get_param(modem, MDMPRM_VOICEINFO);

	switch (num) {
	case SREG_FLASH_TIMER:
		return VCE_FLASH_TIMER;
	case SREG_HANDSET_GANE:
		return VCE_HANDSET_GAIN;
	case SREG_VOICE_DIALTONE_DETECT_DELAY:
		return VCE_DIALTONE_DETECT_DELAY;
	case SREG_SILENCE_DETECT_SENSITIVITY:
		level = vi->silence_detect_sensitivity
			>> VCE_SILENCE_LEVEL_SHIFT;
		if (level == 0)
			return vi->silence_detect_sensitivity != 0;
		if (level > VCE_SILENCE_LEVEL_MAX)
			return VCE_SILENCE_LEVEL_MAX;
		return (int)level;
	case SREG_SILENCE_DETECT_DURATION:
		return (int)vi->silence_detect_period;
	case SREG_MIC_GAIN:
	case SREG_LINE_RECORD_GAIN:
		return (int)vi->rx_gain;
	}
	return 0;
}

/*
 * Build the VOICE service: the 0x1484-byte object, the two rate converters
 * the line rate needs, and the voice service core underneath it.
 *
 * THE RESAMPLER PAIR IS TWO INDEPENDENT TESTS, NOT ONE, and that is what the
 * object does rather than a paraphrase of it: 8000 jumps over both creations
 * (0x76c) and everything else runs two separate `9600? 48000?` ladders, one
 * per direction, storing each result before either is checked.  Written as a
 * single switch it would not produce the object's two `cmp $0x2580` /
 * `cmp $0xbb80` pairs.
 *
 * THE CONFIG BLOCK IS THE CLASS-2 EVIDENCE FOR `voice_config.fn_04`.  The
 * three function pointers are `R_386_32` relocations against `.text` at
 * 0x7e4, 0x7eb and 0x7f0 with targets 0x600, 0x630 and 0x660 -- vce_hook_on,
 * vce_hook_off and vce_get_sreg -- and the slot each lands in is fixed by the
 * store offsets (0x24 -> +0x04, 0x28 -> +0x08, 0x2c -> +0x0c).  So +0x04 is
 * the S-register getter, which is what voice.h's rotation note concluded from
 * the other end.  All three are `t` in the object and external here (F8770);
 * a stored function pointer pins the symbol at link exactly as a call does
 * (F8493), which is why this file must define all three.
 *
 * The failure arm cannot be reached from a fixture -- the only way out of it
 * is a failed `sysdep_malloc` inside `voice_create`, and the harness
 * allocator cannot be made to fail.  What CAN be reached, and is, is the
 * unsupported-rate path: any rate that is not one of the three leaves both
 * converters NULL and takes the same teardown.  F8822's shape, one rung down.
 */
void *
VOICE_create(void *modem, unsigned int rate)
{
	struct vce *v;
	struct voice_config cfg;
	struct rc *rc_in, *rc_out;
	int arg[3];

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("voice: VOICE_create...\n");

	v = (struct vce *)sysdep_malloc(sizeof *v);
	if (!v)
		return 0;
	sysdep_memset(v, 0, sizeof *v);
	v->modem = modem;

	if (rate != VCE_RATE_8000) {
		rc_in = 0;
		if (rate == VCE_RATE_9600)
			rc_in = RcFixed_Create(VCE_RC_MODE_9600_IN);
		else if (rate == VCE_RATE_48000)
			rc_in = RcFixed_Create(VCE_RC_MODE_48000_IN);
		v->rc_in = rc_in;

		rc_out = 0;
		if (rate == VCE_RATE_9600)
			rc_out = RcFixed_Create(VCE_RC_MODE_9600_OUT);
		else if (rate == VCE_RATE_48000)
			rc_out = RcFixed_Create(VCE_RC_MODE_48000_OUT);
		v->rc_out = rc_out;

		if (!rc_in || !rc_out)
			goto fail;
	}

	v->state = VOICE_STATE_COMMAND;
	v->info = (struct voice_info *)modem_get_param(modem,
						       MDMPRM_VOICEINFO);

	cfg.modem = modem;
	cfg.fn_04 = (unsigned int (*)(void *, int))vce_get_sreg;
	cfg.fn_08 = vce_hook_on;
	cfg.fn_0c = vce_hook_off;

	/*
	 * The output ring starts a whole block ahead of the input one -- its
	 * count is primed with `block` and its `blk` selects the SECOND half,
	 * so the first resample writes the half the drain is not reading.
	 * That is the double buffer's phase, set once, here.
	 */
	v->block = rate * VCE_BLOCK_REF_SAMPLES / VCE_BLOCK_REF_RATE;
	v->out_ring.count = (int)v->block;
	v->out_ring.blk = v->block;

	v->voice = voice_create(&cfg);
	if (!v->voice)
		goto fail;

	/*
	 * Two commands into the freshly built core, through one argument
	 * block.  The first is VLS with 0, which is `out_format = 0` -- the
	 * same value the memset already left there.  The second asks for MODE
	 * 4, which is not one of the four modes `voice_command` knows, so it
	 * falls into that switch's default and the only thing it achieves is
	 * `beep_done = 1`.  Deviation D1024.
	 */
	arg[0] = 0;
	voice_command(v->voice, VOICE_VLS_COMMAND, arg);
	arg[0] = 4;
	voice_command(v->voice, VOICE_SET_MODE_COMMAND, arg);

	return v;

fail:
	if (v->rc_in)
		RcFixed_Delete(v->rc_in);
	if (v->rc_out)
		RcFixed_Delete(v->rc_out);
	sysdep_free(v);
	return 0;
}

/*
 * Tear it down.  No NULL check on `obj` -- the object dereferences it two
 * instructions in, at 0x921, so a NULL handle faults here rather than being
 * ignored.  Every field it frees IS guarded.
 *
 * The object has two `jmp sysdep_free` exits, at 0x93e and 0x990, and there
 * is nothing to spell for that: GCC duplicates the whole body along the debug
 * and non-debug edges of the leading `if`, so the trailing tail call comes out
 * twice.  One `sysdep_free(v);` here produces both.
 */
void
VOICE_delete(void *obj)
{
	struct vce *v = (struct vce *)obj;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("voice: VOICE_delete...\n");

	if (v->voice)
		voice_delete(v->voice);
	if (v->rc_in)
		RcFixed_Delete(v->rc_in);
	if (v->rc_out)
		RcFixed_Delete(v->rc_out);
	sysdep_free(v);
}

/*
 * The host's command interface: eight `enum VOICE_CMD` opcodes translated
 * into the four `voice_command` opcodes underneath, with the tone parameters
 * fetched out of `struct voice_info` on the way.
 *
 * SEVEN ARMS CONVERGE ON ONE TAIL, at 0x9ec: call `voice_command` with the
 * translated opcode and the shared argument block, and answer whatever it
 * answered.  Only the refusal leaves early.
 *
 * `host_count = 0` IS NOT PART OF THAT TAIL, and reading it as part of it is
 * the mistake this function invites.  The store is at 0x9e0 and the join is
 * FOUR INSTRUCTIONS LATER at 0x9ec, so only the four state arms -- the ones
 * that fall into 0x9e0 -- clear the field; BEEP, DTMF and ABORT jump straight
 * to 0x9ec, and so does every refusal.  The four copies below are what the
 * compiler cross-jumped into that single store.  No return value can tell the
 * two readings apart; t_voiceapi's whole-object comparison can, and did.
 * Finding F8837.
 *
 * THREE THINGS HERE ARE FAITHFUL AND LOOK WRONG.
 *
 *   - Opcode 3 is inside the accepted range and reports itself unknown.  It
 *     is the host's VOICE_CMD_STATE_DUPLEX and this object has no arm for it;
 *     its jump-table slot is the out-of-range label.  Deviation D1021.
 *   - VOICE_CMD_ABORT passes the argument block UNINITIALISED.  Nothing on
 *     that path writes it and `voice_command`'s VOICE_ABORT_COMMAND arm reads
 *     no argument, so it is harmless -- but it is what the object does and it
 *     is not tidied here.  Deviation D1025.
 *   - Each tone parameter is computed TWICE where the debug line prints it,
 *     once for the printf and once for the argument block.  That is the
 *     compiler rematerialising across the call, not two source expressions;
 *     `info->tone_duration / 10` is written once per arm below.
 *
 * The divide is UNSIGNED -- `mul $0xcccccccd` / `shr $3` at 0xa91 and 0xae3 --
 * which is what `struct voice_info`'s `unsigned` members give for free.
 */
int
VOICE_command(void *obj, unsigned int cmd)
{
	struct vce *v = (struct vce *)obj;
	struct voice_info *info;
	unsigned int dur;
	int arg[3];
	int op;

	if (!v)
		return -1;

	info = v->info;

	switch (cmd) {
	case VOICE_CMD_STATE_COMMAND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "voice: VCE: VOICE_CMD_SET_MODE: COMMAND\n");
		arg[0] = VOICE_MODE_ONLINE;
		v->host_count = 0;
		op = VOICE_SET_MODE_COMMAND;
		break;

	case VOICE_CMD_STATE_RX:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "voice: VCE: VOICE_CMD_SET_MODE: RX\n");
		arg[0] = VOICE_MODE_RX;
		v->host_count = 0;
		op = VOICE_SET_MODE_COMMAND;
		break;

	case VOICE_CMD_STATE_TX:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "voice: VCE: VOICE_CMD_SET_MODE: TX\n");
		arg[0] = VOICE_MODE_TX;
		v->host_count = 0;
		op = VOICE_SET_MODE_COMMAND;
		break;

	case VOICE_CMD_STATE_SPEAKER:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "voice: VCE: VOICE_CMD_SET_MODE: SPEAKER\n");
		arg[0] = VOICE_MODE_DUPLEX;
		v->host_count = 0;
		op = VOICE_SET_MODE_COMMAND;
		break;

	case VOICE_CMD_BEEP:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "voice: VCE: VOICE_CMD_BEEP, %d %d %d\n",
			    info->tone1_freq, info->tone2_freq,
			    info->tone_duration / 10);
		arg[0] = (int)info->tone1_freq;
		arg[1] = (int)info->tone2_freq;
		arg[2] = (int)(info->tone_duration / 10);
		op = VOICE_BEEP_COMMAND;
		break;

	case VOICE_CMD_DTMF:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "voice: VCE: VOICE_CMD_DTMF, %d %d\n",
			    info->dtmf_symbol, info->tone_duration / 10);
		arg[0] = (int)info->dtmf_symbol;
		/*
		 * A duration under 10 ms would round to nothing, so it is
		 * floored at one unit -- and only the ARGUMENT is floored;
		 * the debug line above prints the raw quotient.
		 */
		dur = info->tone_duration / 10;
		if (dur == 0)
			dur = 1;
		arg[1] = (int)dur;
		op = VOICE_DTMF_COMMAND;
		break;

	case VOICE_CMD_ABORT:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("voice: VCE: VOICE_CMD_ABORT\n");
		op = VOICE_ABORT_COMMAND;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "voice: VCE: Unknown command %u\n", cmd);
		return -1;
	}

	return voice_command(v->voice, op, arg);
}

/*
 * One buffer of line audio through the voice service.
 *
 * THE SHAPE.  An outer loop that takes the caller's buffers a block at a
 * time, an inner loop that moves samples between those buffers and the two
 * rate-conversion rings a memcpy at a time, and -- whenever the input ring
 * has a whole block -- the actual work: resample in, scale to float, exchange
 * host bytes, run `voice_modem`, act on the message it answers, scale back
 * and resample out.
 *
 * WHY THE INNER LOOP CLAMPS THREE TIMES.  `n` may not exceed the block, what
 * the caller still has, or the distance from either ring's cursor to the end
 * of its 2*block window -- the last two because a memcpy must not straddle
 * the wrap.  All three comparisons are UNSIGNED in the object and are
 * unsigned here for the same reason: `block` is an unsigned member, so it
 * converts everything it meets.
 *
 * THE OUTER LOOP ADVANCES THE CALLER'S POINTERS BY HALF WHAT IT CONSUMED.
 * The inner loop moves `2 * n` BYTES per step and advances its own copies by
 * `2 * n`; the outer step at 0xf69-0xf7b adds `chunk` -- not `2 * chunk` --
 * to `in` and `out`, which are `void *` in the host's own prototype and so
 * take byte arithmetic.  Every iteration after the first therefore re-reads
 * and re-writes the second half of the block it just handled.  It is a defect
 * in the original, it is reproduced rather than repaired, and it is reachable
 * at every rate: the outer loop runs twice as soon as `count` exceeds the
 * block, which is 160 samples at 8 kHz.  Deviation D1020.
 *
 * THE 8 kHz CASE MOVES NO AUDIO AT ALL.  At the pump's own rate `VOICE_create`
 * builds neither converter, and `RcFixed_Resample` on a NULL handle stores 0
 * through `out_count` and returns -- so `rlen` is 0, both scaling loops are
 * empty and the output ring is never written.  The host I/O and the whole
 * message machine still run.  Deviation D1022.
 */
int
VOICE_process(void *obj, void *in, void *out, int count)
{
	struct vce *v = (struct vce *)obj;
	int ret = 0;

	while (count > 0) {
		unsigned char *rx = (unsigned char *)in;
		unsigned char *tx = (unsigned char *)out;
		int chunk = count;
		int pending = 0;
		int remaining;

		/*
		 * The three casts in this function are written rather than
		 * left implicit.  Each names the conversion the language
		 * performs anyway -- `block`, `wr` and `rd` are unsigned
		 * members, so every comparison against them is unsigned, and
		 * the object encodes exactly that (`jbe` at 0xc1a, 0xcb9 and
		 * `jb` at 0xd32).  A cast to the type the usual arithmetic
		 * conversions already produce cannot move code generation; it
		 * is here so the unsignedness is visible to a reader and is
		 * not "corrected" by someone reading a signed count.
		 */
		if ((unsigned int)chunk > v->block)
			chunk = (int)v->block;
		remaining = chunk;

		while (remaining > 0) {
			unsigned int n = v->block;

			if (n > (unsigned int)remaining)
				n = (unsigned int)remaining;
			if (n > 2 * v->block - v->in_ring.wr)
				n = 2 * v->block - v->in_ring.wr;
			if (n > 2 * v->block - v->out_ring.rd)
				n = 2 * v->block - v->out_ring.rd;

			sysdep_memcpy(&v->in_ring.data[v->in_ring.wr], rx,
				      2 * n);
			rx += 2 * n;
			v->in_ring.count += (int)n;
			v->in_ring.wr = (v->in_ring.wr + n) % (2 * v->block);

			if ((unsigned int)v->in_ring.count >= v->block) {
				int rlen = VCE_BLOCK_REF_SAMPLES;
				int outlen;
				int msg, code, i;
				unsigned short hostcount, blkcount;

				RcFixed_Resample(v->rc_in,
						 &v->in_ring.data[v->in_ring.blk],
						 (int)v->block, v->lin, &rlen);

				for (i = 0; i < rlen; i++)
					v->from_line[i] =
					    (float)(v->lin[i] * VCE_LINE_IN_SCALE);

				blkcount = VCE_BLOCK_REF_SAMPLES;

				/*
				 * Playback pulls whatever the host still owes
				 * us; recording pulls a block only to watch
				 * for <DLE>'!'.  The two are separate tests
				 * and the object runs both.
				 */
				hostcount = (v->state == VOICE_STATE_TX)
					    ? (unsigned short)v->host_count : 0;
				if (hostcount != 0)
					hostcount = (unsigned short)
					    modem_recv_from_tty(v->modem,
								v->host_in,
								v->host_count);
				if (v->state == VOICE_STATE_RX) {
					int got = modem_recv_from_tty(
					    v->modem, v->host_in,
					    VCE_BLOCK_REF_SAMPLES);

					if (got > 1) {
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: input in RX mode: %d (%d,%d)\n",
							    got, v->host_in[0],
							    v->host_in[1]);
						if (*(const short *)v->host_in
						    == VCE_HOST_ABORT_WORD)
							VOICE_command(v,
							    VOICE_CMD_ABORT);
					}
				}

				msg = voice_modem(v->voice,
						  (short *)v->host_in,
						  v->to_line, v->from_line,
						  (short *)v->host_out,
						  &hostcount, &blkcount);

				if (v->state == VOICE_STATE_RX
				    && blkcount != 0)
					modem_send_to_tty(v->modem,
							  v->host_out,
							  blkcount);

				v->host_count = hostcount;
				code = 0;

				/*
				 * Only a CHANGE of message is acted on, which
				 * is what makes the report characters fire
				 * once per event rather than once per block.
				 */
				if (v->last_message != msg) {
					switch (msg) {
					case VOICE_NO_MESSAGE:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE: VOICE_NO_MESSAGE\n");
						break;

					case VOICE_OK:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE:VOICE_OK\n");
						code = VOICE_STATUS_OK;
						v->state = VOICE_STATE_COMMAND;
						break;

					case VOICE_START_ONLINE:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE: VOICE_START_ONLINE\n");
						v->state = VOICE_STATE_COMMAND;
						code = VOICE_STATUS_OK;
						break;

					case VOICE_START_TX:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE:VOICE_START_TX\n");
						v->state = VOICE_STATE_TX;
						code = VOICE_STATUS_CONNECT;
						v->host_count =
						    VCE_BLOCK_REF_SAMPLES;
						break;

					case VOICE_START_RX:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE:VOICE_START_RX\n");
						v->state = VOICE_STATE_RX;
						code = VOICE_STATUS_CONNECT;
						break;

					case VOICE_START_DUPLEX:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE:VOICE_START_DUPLEX\n");
						v->state = VOICE_STATE_DUPLEX;
						code = VOICE_STATUS_CONNECT;
						break;

					case VOICE_PURGE:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE:VOICE_PURGE\n");
						break;

					case VOICE_ERROR:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE:VOICE_ERROR\n");
						code = VOICE_STATUS_ERROR;
						v->state = VOICE_STATE_COMMAND;
						break;

					case VOICE_START_ONLINE_AFTER_ABORT:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE: VOICE_START_ONLINE_AFTER_ABORT(1)\n");
						v->state = VOICE_STATE_COMMAND;
						code = VOICE_STATUS_OK;
						break;

					case VOICE_CANCEL:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE:VOICE_CANCEL\n");
						break;

					case VOICE_BUSY: {
						unsigned char rep[2] = {
							VOICE_DLE
						};

						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE: BUSY\n");
						rep[1] = VOICE_REPORT_BUSY;
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: report char: '%c' (%d)\n",
							    rep[1], rep[1]);
						modem_send_to_tty(v->modem,
								  rep, 2);
						break;
					}

					case VOICE_DIALTONE: {
						unsigned char rep[2] = {
							VOICE_DLE
						};

						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE: DIALTONE\n");
						rep[1] = VOICE_REPORT_DIALTONE;
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: report char: '%c' (%d)\n",
							    rep[1], rep[1]);
						modem_send_to_tty(v->modem,
								  rep, 2);
						break;
					}

					case VOICE_FAX_TONE: {
						unsigned char rep[2] = {
							VOICE_DLE
						};

						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE: FAX Tone\n");
						rep[1] = VOICE_REPORT_FAX_TONE;
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: report char: '%c' (%d)\n",
							    rep[1], rep[1]);
						modem_send_to_tty(v->modem,
								  rep, 2);
						break;
					}

					case VOICE_UNDERRUN: {
						unsigned char rep[2] = {
							VOICE_DLE
						};

						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRM_VCE: Underrun\n");
						rep[1] = VOICE_REPORT_UNDERRUN;
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: report char: '%c' (%d)\n",
							    rep[1], rep[1]);
						modem_send_to_tty(v->modem,
								  rep, 2);
						break;
					}

					default:
						/*
						 * Shares VOICE_ERROR's whole
						 * effect -- 0xf9d falls into
						 * 0xe79 -- and only the line
						 * it prints is its own.
						 */
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "voice: STRMVCE Monitor: Unknown message %d\n",
							    msg);
						code = VOICE_STATUS_ERROR;
						v->state = VOICE_STATE_COMMAND;
						break;
					}
					v->last_message = msg;
				}

				for (i = 0; i < rlen; i++)
					v->lin[i] = (short)(v->to_line[i]
						    * VCE_LINE_OUT_SCALE);

				outlen = (int)v->block;
				RcFixed_Resample(v->rc_out, v->lin, rlen,
						 &v->out_ring.data[v->out_ring.blk],
						 &outlen);

				if (code != 0)
					pending = code;

				/*
				 * Retire the block from one ring, hand it to
				 * the other, and flip both halves.
				 */
				v->in_ring.count -= (int)v->block;
				v->in_ring.blk = v->in_ring.blk ? 0 : v->block;
				v->out_ring.count += (int)v->block;
				v->out_ring.blk = v->out_ring.blk ? 0
						  : v->block;
			}

			sysdep_memcpy(tx, &v->out_ring.data[v->out_ring.rd],
				      2 * n);
			tx += 2 * n;
			remaining -= (int)n;
			v->out_ring.count -= (int)n;
			v->out_ring.rd = (v->out_ring.rd + n) % (2 * v->block);
		}

		if (pending != 0)
			ret = pending;

		count -= chunk;
		in = (unsigned char *)in + chunk;
		out = (unsigned char *)out + chunk;
	}

	return ret;
}

/*
 * The FDSP environment the voice stream runs in: two echo delays, in
 * samples, WRITTEN not read.  Both are constants in this object -- 51 and
 * 369 -- and the incoming values are only ever printed, which is what makes
 * the two debug lines ("old:" before, "new:" after) the whole evidence for
 * the argument names.  A caller therefore cannot influence the answer.
 */
void
STRM_VCE_GetFDSPEnvironmentalParams(short *psFarEchoDelay,
				    short *psNearEchoDelay)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "voice: StrmVCE old: *psFarEchoDelay %d ,*psNearEchoDelay %d \n",
		    *psFarEchoDelay, *psNearEchoDelay);

	*psFarEchoDelay = STRM_VCE_FAR_ECHO_DELAY;
	*psNearEchoDelay = STRM_VCE_NEAR_ECHO_DELAY;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "voice: StrmVCE new: *psFarEchoDelay %d ,*psNearEchoDelay %d \n",
		    *psFarEchoDelay, *psNearEchoDelay);
}

void *
RD_create(void *modem, unsigned int rate)
{
	struct rd *rd;
	struct ring_detector_cfg cfg;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("RD: create...\n");

	if (rate != RD_RATE_8000 && rate != RD_RATE_9600)
		return 0;

	rd = (struct rd *)sysdep_malloc(sizeof *rd);
	if (!rd)
		return 0;
	sysdep_memset(rd, 0, sizeof *rd);
	rd->modem = modem;

	cfg.fs = (int)rate;
	cfg.min_freq = 15;
	cfg.max_freq = 80;
	cfg.min_on_dur = 120;
	cfg.min_off_dur = 120;

	switch ((int)modem_get_param(modem, MDMPRM_CODECTYPE)) {
	case 4:
	case 12:
		cfg.threshold = 1000;
		break;
	case 13:
	case 15:
		cfg.threshold = 650;
		break;
	case 14:
		cfg.threshold = 850;
		break;
	default:
		cfg.threshold = RD_THRESHOLD_DEFAULT;
		break;
	}

	rd->det = RingDetector_Create(&cfg);
	if (!rd->det) {
		sysdep_free(rd);
		return 0;
	}
	return rd;
}

void
RD_delete(void *obj)
{
	struct rd *rd = (struct rd *)obj;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("RD: delete...\n");
	RingDetector_Delete(rd->det);
	sysdep_free(rd);
}

int
RD_process(void *obj, void *in, int count)
{
	struct rd *rd = (struct rd *)obj;
	int ret;

	ret = RingDetector_Process(rd->det, (short *)in, (unsigned int)count);
	if (ret) {
		int freq, duration;

		RingDetector_GetLastRing(rd->det, &freq, &duration);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("RD: RD: freq = %d, duration = %d\n",
					     freq, duration);
	}
	return ret;
}

/*
 * slmodemd declares the two out-parameters `long *`; the object stores 32-bit
 * words through them, which is the same thing on the ILP32 target it was
 * built for and not the same thing anywhere else.  `int *` is what the
 * instructions say, so `int *` is what is written -- the same call this tree
 * already made for `dsp_info::clock_deviation`.
 */
void
RD_ring_details(void *obj, int *freq, int *duration)
{
	struct rd *rd = (struct rd *)obj;

	RingDetector_GetLastRing(rd->det, freq, duration);
}

void
RingDetector_Delete(struct ring_detector *s)
{
	if (s)
		sysdep_free(s);
}

/*
 * Program the soft ring detector from a configuration block.
 *
 * The clamps and the derived shorts are the object's, including two things
 * that look odd and are faithful:
 *   - min_off_dur is clamped to >= 120 TWICE when the threshold is
 *     negative -- once with everything else and once again at the end;
 *   - idle_debounce divides by fs/80, so an fs below 80 divides by zero.  The
 *     object does exactly that; callers evidently never hand it one.
 *
 * A negative threshold selects the second mode (lock_debounce/lock_level =
 * 0/200 rather than 2/100) and is used through its absolute value
 * everywhere else.
 */
void
RingDetector_Reset(struct ring_detector *s, struct ring_detector_cfg *c)
{
	int thr = c->threshold;
	int athr = thr < 0 ? -thr : thr;

	s->ring_active = 0;
	s->idle_debounce = (short)(athr / (c->fs / 80));
	s->fs = c->fs;
	s->guard_limit = (short)(3 * c->fs / (4 * c->min_freq));
	s->min_freq = c->min_freq;
	s->max_freq = c->max_freq;
	s->min_on_dur = c->min_on_dur;
	s->min_off_dur = c->min_off_dur;
	if (c->min_freq <= 13)
		s->min_freq = 14;
	if (s->max_freq > 100)
		s->max_freq = 100;
	if (s->min_on_dur <= 39)
		s->min_on_dur = 40;
	if (s->min_off_dur <= 119)
		s->min_off_dur = 120;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "\n Reset Soft Ring: Threshold = %d, Fs = %d, MinFreq = %d, MaxFreq =%d \n\tminOnDur = %d, minOffDur = %d \n",
		    c->threshold, s->fs, s->min_freq, s->max_freq,
		    s->min_on_dur, s->min_off_dur);

	s->report_samples = 0;
	s->half_samples = 0;
	s->idle_samples = 0;
	s->freq_avg = 0;
	s->last_freq = 0;
	s->freq_n = 0;
	s->ring_reported = 0;
	s->state = RD_STATE_SEARCH;
	s->above_run = 0;
	s->below_run = 0;
	athr = c->threshold < 0 ? -c->threshold : c->threshold;
	s->threshold = (short)athr;
	s->upper_level = (short)athr;
	s->lower_level = (short)-athr;
	s->guard_run = 0;
	s->cycles = 0;
	s->cross_samples = 0;
	s->band_samples = 0;
	s->above_need = s->idle_debounce;
	s->below_need = s->idle_debounce;
	if (c->threshold < 0) {
		s->lock_debounce = 0;
		s->lock_level = 200;
		if (s->min_off_dur <= 119)
			s->min_off_dur = 120;
	} else {
		s->lock_debounce = 2;
		s->lock_level = 100;
	}
}

/*
 * No NULL check on the allocation: the object dereferences the returned
 * pointer on the very next instruction (`movw $0x0,0x3a(%esi)`), so a failed
 * malloc faults here rather than being reported.  Reproduced, not repaired --
 * `RD_create`'s test of the result is therefore unreachable in the object too.
 */
struct ring_detector *
RingDetector_Create(struct ring_detector_cfg *c)
{
	struct ring_detector *s;

	s = (struct ring_detector *)sysdep_malloc(sizeof *s);
	RingDetector_Reset(s, c);
	return s;
}

void
RingDetector_GetLastRing(struct ring_detector *s, int *freq, int *dur)
{
	*freq = s->last_freq;
	*dur = s->last_duration;
}

/*
 * One completed cycle: measure its frequency, fold it into the running mean
 * if it is inside [min_freq, max_freq], and count it.
 *
 * `half` is the half-cycle length in samples PLUS ONE, which is what the
 * object divides by -- so the estimate is fs/(half_samples+1) and never
 * divides by zero.
 */
static void
rd_measure(struct ring_detector *s, int half, int min_freq)
{
	int f = s->fs / half;

	if (f < s->max_freq + 1 && f >= min_freq) {
		int n = s->freq_n;

		s->freq_n = n + 1;
		s->freq_avg = (short)((s->freq_avg * n + f) / (n + 1));
	}
	s->cycles++;
	s->half_samples = 0;
}

/*
 * A sample inside the comparator's guard band -- past the crossing level but
 * short of full amplitude.  Tolerated for up to three quarters of a period at
 * min_freq; beyond that the tone has gone and the detector starts again.
 */
static void
rd_guard(struct ring_detector *s, int band)
{
	s->above_run = 0;
	s->below_run = 0;
	if (++s->guard_run > s->guard_limit) {
		s->cycles = 0;
		s->state = RD_STATE_SEARCH;
		s->idle_samples = s->guard_limit;
		s->guard_run = 0;
		s->above_need = s->idle_debounce;
		s->below_need = s->idle_debounce;
		s->cross_samples = 0;
		s->band_samples = 0;
	} else {
		s->band_samples = band;
	}
}

int
RingDetector_Process(struct ring_detector *s, short *in, unsigned int count)
{
	int min_freq = s->min_freq;
	int off_samples = s->fs * s->min_off_dur / 1000;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < count; i++) {
		int period;

		s->report_samples++;

		if (s->state == RD_STATE_SEARCH) {
			short thr = s->threshold;
			int idle = s->idle_samples + 1;
			short sample;

			s->half_samples = 0;
			sample = in[i];
			if (sample > thr) {
				s->below_run = 0;
				if (++s->above_run > s->above_need) {
					s->state = RD_STATE_HIGH;
					s->upper_level = thr;
					s->below_need = s->lock_debounce;
					s->lower_level = s->lock_level;
					s->idle_samples = 0;
				} else {
					s->idle_samples = idle;
				}
			} else if (sample < -thr) {
				s->above_run = 0;
				if (++s->below_run > s->below_need) {
					s->state = RD_STATE_LOW;
					s->upper_level = (short)-s->lock_level;
					s->above_need = s->lock_debounce;
					s->lower_level = (short)-thr;
					s->idle_samples = 0;
				} else {
					s->idle_samples = idle;
				}
			} else {
				s->idle_samples = idle;
				s->above_run = 0;
				s->below_run = 0;
			}
		} else if (s->state == RD_STATE_HIGH) {
			int half = s->half_samples + 1;
			int band = s->band_samples + 1;
			short sample;

			s->cross_samples++;
			sample = in[i];
			if (sample >= s->lower_level) {
				s->half_samples = half;
				if (sample >= s->upper_level) {
					s->band_samples = band;
					s->above_run = 0;
					s->below_run = 0;
				} else {
					rd_guard(s, band);
				}
			} else {
				s->above_run = 0;
				if (++s->below_run > s->below_need) {
					if (s->below_need == s->idle_debounce)
						rd_measure(s, half, min_freq);
					else
						s->half_samples = half;
					s->state = RD_STATE_LOW;
					s->guard_run = 0;
					s->cross_samples = 0;
					s->band_samples = 0;
				} else {
					s->band_samples = band;
					s->half_samples = half;
				}
			}
		} else if (s->state == RD_STATE_LOW) {
			int half = s->half_samples + 1;
			int band = s->band_samples + 1;
			short sample;

			s->cross_samples++;
			sample = in[i];
			if (sample <= s->upper_level) {
				s->half_samples = half;
				if (sample <= s->lower_level) {
					s->band_samples = band;
					s->above_run = 0;
					s->below_run = 0;
				} else {
					rd_guard(s, band);
				}
			} else {
				s->below_run = 0;
				if (++s->above_run > s->above_need) {
					if (s->above_need == s->idle_debounce)
						rd_measure(s, half, min_freq);
					else
						s->half_samples = half;
					s->state = RD_STATE_HIGH;
					s->guard_run = 0;
					s->cross_samples = 0;
					s->band_samples = 0;
				} else {
					s->band_samples = band;
					s->half_samples = half;
				}
			}
		}

		/*
		 * A whole period at min_freq with no crossing means the tone
		 * has gone: back to SEARCH, with the idle counter seeded to
		 * that period so the "ring has ended" test below sees it.
		 */
		period = s->fs / min_freq;
		if (s->cross_samples > period) {
			s->idle_samples = period;
			s->cycles = 0;
			s->guard_run = 0;
			s->cross_samples = 0;
			s->state = RD_STATE_SEARCH;
			s->above_need = s->idle_debounce;
			s->below_need = s->idle_debounce;
		}

		/*
		 * cycles/freq_avg is the tone's length in seconds.  Once it
		 * passes minOnDur this is a ring, and the reported frequency
		 * is zeroed so the caller reads the edge as a ring STARTING.
		 */
		if (s->freq_avg != 0 && s->cycles > 0 &&
		    s->cycles * 1000 / s->freq_avg > s->min_on_dur) {
			s->ring_active = 1;
			s->last_freq = 0;
		}

		/*
		 * minOffDur of silence, measured either in SEARCH or between
		 * crossings, ends the ring.  The measured frequency is
		 * published on the way out, which is what makes the closing
		 * edge report a positive one.
		 */
		if (s->idle_samples > off_samples ||
		    s->band_samples > off_samples) {
			if (s->freq_avg != 0)
				s->last_freq = s->freq_avg;
			s->freq_avg = 0;
			s->freq_n = 0;
			s->ring_active = 0;
		}
	}

	/*
	 * The verdict is only handed out when it changes, and the duration is
	 * the time spent in the state being left -- corrected by the
	 * difference between the two minimum durations and 20 ms, then held
	 * at the minimum for the state being left.
	 */
	if (s->ring_reported != s->ring_active) {
		int ms;

		if (s->ring_reported == 1) {
			ms = s->min_on_dur - s->min_off_dur + 20
			     + s->report_samples * 1000 / s->fs;
			if (ms < s->min_on_dur)
				ms = s->min_on_dur;
		} else {
			ms = s->min_off_dur - s->min_on_dur - 20
			     + s->report_samples * 1000 / s->fs;
			if (ms < s->min_off_dur)
				ms = s->min_off_dur;
		}
		s->last_duration = ms;
		s->report_samples = 0;
		ret = 1;
	}
	s->ring_reported = s->ring_active;
	return ret;
}

/*
 * `.text` 0x001450, 172 bytes -- see `fax.h` for `struct fax_ctx` and why
 * this sits in a separate header from `struct voice_ctx` despite living in
 * the same TU.  `FAX_create` and `FAX_class1_command`, the object's own
 * neighbours on either side of this group (0x001500 and 0x001740), are now
 * both written too (F10121), below.
 *
 * The object's own order: print "fax: delete...\n" (`.rodata.str1.1`
 * 0x1be) when `dsplibs_debug_level > 1`, then unconditionally check and
 * delete `rc_a`, `rc_b` and `class1` in that order, clear `class1`, and
 * free `ctx`.  Every check is independent -- there is no early return, and
 * the debug branch rejoins the same three checks rather than skipping any
 * of them.
 */
void
FAX_delete(struct fax_ctx *ctx)
{
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("fax: delete...\n");

	if (ctx->rc_a != NULL)
		RcFixed_Delete(ctx->rc_a);
	if (ctx->rc_b != NULL)
		RcFixed_Delete(ctx->rc_b);
	if (ctx->class1 != NULL)
		fax_class1_delete(ctx->class1);

	ctx->class1 = NULL;
	sysdep_free(ctx);
}

/*
 * slmodemd's own S-register reader -- NOT the voice service's own
 * `voice_get_sreg`-shaped function a few hundred lines up in this same file
 * (that one's own banner already says so: "this is NOT a call into it").
 * `FAX_create` is the one traced caller, asking for register 7 (T.30's own
 * S7, the carrier-wait timeout `struct fax_class1_cfg::s7_timeout` carries
 * straight through).  No dsplib header owns it, the same reason
 * `modem_recv_from_tty`/`modem_send_to_tty` are declared locally above.
 */
extern long modem_get_sreg(void *modem, unsigned int reg);

/*
 * `.text` 0x001500, 564 bytes.  See `fax.h`'s own prototype comment for
 * `originate`/`rate`'s meaning; this banner is the CONTROL-FLOW account.
 *
 *   1. Debug print ("fax: create...\n", `.rodata.str1.1` 0x1ce) at debug
 *      level > 1, BEFORE anything else -- even the allocation.
 *   2. `sysdep_malloc(sizeof(struct fax_ctx))`; NULL returns NULL directly
 *      (no debug line, no cleanup -- there is nothing yet to clean up).
 *      `sysdep_memset` the whole thing to 0, then `ctx->modem = modem`.
 *   3. `rate == 8000`: skip step 4 entirely, `rc_a`/`rc_b` stay NULL (the
 *      memset's own zero). Any other value: build `rc_a` (9600 -> converter
 *      3, 48000 -> converter 5, anything else -> NULL) and, if that
 *      succeeded, `rc_b` the same way (9600 -> 2, 48000 -> 4). A NULL
 *      resampler where one was expected -- including the "anything else"
 *      case, which builds neither and always fails here -- jumps straight
 *      to the teardown at step 6.
 *   4. `ctx->host_frame_samples = ctx->out_produced = ctx->out_write_half =
 *      rate * CLASS1_BLOCK_SAMPLES / 8000` (unsigned; the object's own
 *      `mul`/`shr` reciprocal, re-derived rather than assumed to be `/50`
 *      even though the arithmetic reduces to that for every rate this
 *      function accepts).
 *   5. Build the Class 1 session: `local` is a `struct fax_class1_cfg`,
 *      zeroed, with `mode` from `originate` (nonzero ->
 *      `CLASS1_ANS_ORG_NORMAL`, zero -> `CLASS1_ANS_ORG_ANSWER`),
 *      `s7_timeout` from `modem_get_sreg(modem, 7)`, and `iir_enable = 1`
 *      (`answer_tone_ms`/`f08`/`disable_cng` all stay 0, the zeroed
 *      default). Debug-print "fax: fax_class1 will created (ans_org=%d,
 *      s7=%d)\n" at level > 1 with `local.mode` and the raw S7 value, THEN
 *      `ctx->class1 = fax_class1_create(NULL, &local)`. A NULL result also
 *      falls to step 6.
 *   6. TEARDOWN, only reached by a step-3/5 failure: debug-print "fax:
 *      delete...\n" (`FAX_delete`'s own string) at level > 1, delete `rc_a`/
 *      `rc_b`/`class1` exactly as `FAX_delete` does -- INLINE, not by
 *      calling it (no relocation to `FAX_delete` in this range) -- and
 *      `sysdep_free(ctx)`.  Returns NULL.
 *   7. Otherwise returns `ctx`.
 */
struct fax_ctx *
FAX_create(void *modem, int originate, unsigned int rate)
{
	struct fax_ctx *ctx;
	struct fax_class1_cfg local;
	int s7;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("fax: create...\n");

	ctx = sysdep_malloc(sizeof(struct fax_ctx));
	if (ctx == NULL)
		return NULL;
	sysdep_memset(ctx, 0, sizeof(struct fax_ctx));
	ctx->modem = modem;

	if (rate != 8000) {
		if (rate == 9600)
			ctx->rc_a = RcFixed_Create(3);
		else if (rate == 48000)
			ctx->rc_a = RcFixed_Create(5);
		else
			ctx->rc_a = NULL;
		if (ctx->rc_a == NULL)
			goto fail;

		if (rate == 9600)
			ctx->rc_b = RcFixed_Create(2);
		else if (rate == 48000)
			ctx->rc_b = RcFixed_Create(4);
		else
			ctx->rc_b = NULL;
		if (ctx->rc_b == NULL)
			goto fail;
	}

	ctx->host_frame_samples =
	    (int)((rate * (unsigned int)CLASS1_BLOCK_SAMPLES) / 8000U);
	ctx->out_produced = ctx->host_frame_samples;
	ctx->out_write_half = ctx->host_frame_samples;

	sysdep_memset(&local, 0, sizeof(local));
	s7 = modem_get_sreg(modem, 7);
	local.mode = (originate != 0) ? CLASS1_ANS_ORG_NORMAL
				      : CLASS1_ANS_ORG_ANSWER;
	local.s7_timeout = s7;
	local.iir_enable = 1;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(
		    "fax: fax_class1 will created (ans_org=%d, s7=%d)\n",
		    local.mode, s7);

	ctx->class1 = fax_class1_create(NULL, &local);
	if (ctx->class1 != NULL)
		return ctx;

fail:
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("fax: delete...\n");
	if (ctx->rc_a != NULL)
		RcFixed_Delete(ctx->rc_a);
	if (ctx->rc_b != NULL)
		RcFixed_Delete(ctx->rc_b);
	if (ctx->class1 != NULL)
		fax_class1_delete(ctx->class1);
	ctx->class1 = NULL;
	sysdep_free(ctx);
	return NULL;
}

/*
 * `.text` 0x001740, 708 bytes.  `cmd` is one of the six `FAXC1_*` codes
 * (`fax.h`), a DIFFERENT numbering from `fax_class1_command`'s own
 * `FAX_CLASS1_*_COMMAND` (class1.h) that this function remaps into via its
 * own `switch`.  `arg` rides through as a plain int (`(int)(long)arg`,
 * never dereferenced) -- a T.30 rate code for FTM/FRM, or a raw millisecond/
 * sample count for FTS/FRS (`fax_class1_command`'s own `arg3`).
 *
 *   1. `ctx == NULL || ctx->class1 == NULL` returns -1 immediately.
 *   2. Debug print ("fax: FAX_class1_command: %x\n", `cmd`) at level > 1,
 *      unconditionally, before validating `cmd` at all.
 *   3. `(unsigned)cmd > 5`: debug print ("fax: bad command: %x\n") at
 *      level > 1, return -1.
 *   4. Otherwise dispatch on `cmd`:
 *      FAXC1_FTS/FAXC1_FRS: no validation on `rate` at all; debug print
 *        ("fax:  FAXC1_FTS, %x\n"/"...FRS...") at level > 1 with the raw
 *        value, remap to FAX_CLASS1_TS_COMMAND/RS_COMMAND.
 *      FAXC1_FTM/FAXC1_FRM: debug print FIRST (unconditionally, if level >
 *        1 -- FTM's own print always shows 0 for its second `%d`, since
 *        `extra` is not yet computed at that point), THEN validate `rate`
 *        against the twelve T.30 codes `_set_modem_rate` recognises
 *        (0x18/0x30/0x48/0x49/0x4a/0x60/0x61/0x62/0x79/0x7a/0x91/0x92);
 *        anything else returns -1.  FTM alone also sets a fourth argument
 *        to the LITERAL 80 (`fax_class1_command`'s own `arg4`, read only by
 *        its TM command, into `silence_blocks`) -- every other remap leaves
 *        that argument at whatever this function's own caller happened to
 *        leave in the register, a genuinely uninitialised value the object
 *        itself never reads back for those five commands.
 *      FAXC1_FTH/FAXC1_FRH: debug print FIRST at level > 1, THEN require
 *        `rate == 3` exactly (the V.21 control-channel sentinel); anything
 *        else returns -1.
 *   5. `fax_class1_command(ctx->class1, remapped_cmd, rate, extra)`'s own
 *      return is DISCARDED; this function always returns 1 on a validated
 *      dispatch.
 */
int
FAX_class1_command(struct fax_ctx *ctx, int cmd, void *arg)
{
	int rate = (int)(long)arg;
	int inner_cmd;
	int extra;

	if (ctx == NULL || ctx->class1 == NULL)
		return -1;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("fax: FAX_class1_command: %x\n", cmd);

	if ((unsigned)cmd > 5) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax: bad command: %x\n", cmd);
		return -1;
	}

	switch (cmd) {
	case FAXC1_FTS:
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FTS, %x\n", rate);
		inner_cmd = FAX_CLASS1_TS_COMMAND;
		break;

	case FAXC1_FRS:
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FRS, %x\n", rate);
		inner_cmd = FAX_CLASS1_RS_COMMAND;
		break;

	case FAXC1_FTM:
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FTM, %x, %d\n",
					     rate, 0);
		if (rate != 0x18 && rate != 0x30 && rate != 0x48 &&
		    rate != 0x49 && rate != 0x4a && rate != 0x60 &&
		    rate != 0x61 && rate != 0x62 && rate != 0x79 &&
		    rate != 0x7a && rate != 0x91 && rate != 0x92)
			return -1;
		inner_cmd = FAX_CLASS1_TM_COMMAND;
		extra = 0x50;
		break;

	case FAXC1_FRM:
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FRM, %x\n", rate);
		if (rate != 0x18 && rate != 0x30 && rate != 0x48 &&
		    rate != 0x49 && rate != 0x4a && rate != 0x60 &&
		    rate != 0x61 && rate != 0x62 && rate != 0x79 &&
		    rate != 0x7a && rate != 0x91 && rate != 0x92)
			return -1;
		inner_cmd = FAX_CLASS1_RM_COMMAND;
		break;

	case FAXC1_FTH:
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FTH, %x\n", rate);
		if (rate != 3)
			return -1;
		inner_cmd = FAX_CLASS1_TH_COMMAND;
		break;

	default:	/* FAXC1_FRH */
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FRH %x\n", rate);
		if (rate != 3)
			return -1;
		inner_cmd = FAX_CLASS1_RH_COMMAND;
		break;
	}

	(void)fax_class1_command(ctx->class1, inner_cmd, rate, extra);
	return 1;
}

/*
 * `.text` 0x001a10, 1,809 bytes.  See `fax.h`'s struct banner for the
 * field-by-field evidence on every `fax_ctx` member this function reaches;
 * this banner is the CONTROL-FLOW account.
 *
 * OUTER LOOP: while `count > 0`, take `chunk = min(count, ctx->
 * host_frame_samples)` samples' worth of `in`/`out`, process them (below),
 * then `count -= chunk` and advance `in`/`out` by `chunk` -- UNSCALED, the
 * same raw value subtracted from `count`, which is why `in`/`out` are
 * `void *` (`fax.h`'s own note).
 *
 * INNER LOOP, once per outer chunk: copy up to `host_frame_samples` samples
 * from the current `in` position into `in_ring` at `in_write_cursor`
 * (`sysdep_memcpy`), bounded so the copy never runs past `in_ring`'s own
 * end (`2 * host_frame_samples` samples) OR past `out_ring`'s end at the
 * `out_read_cursor` THIS OUTER ITERATION STARTED WITH -- that snapshot is
 * taken once per outer iteration and reused for every inner pass even
 * though the live cursor advances each time, a faithfully-reproduced
 * property of the object and not resolved further.  `in_pending` and
 * `in_write_cursor` are updated to match.
 *
 * When `in_pending` reaches a full `host_frame_samples`, FLUSH:
 *
 *   - Resample `in_ring` (at `in_read_half`) into `rx_resampled` via
 *     `RcFixed_Resample`/`rc_a` when `rc_a != NULL`; when NULL, use the
 *     `in_ring` position directly as `rx` (identity: no rate conversion
 *     needed) and pass `out_ring` (at `out_write_half`) directly as `tx`
 *     instead of the `tx_pump_rate` scratch.
 *   - If the resample's own output count didn't come back as exactly 0xa0
 *     (only reachable when `rc_a != NULL` and `host_frame_samples != 0xa0`,
 *     an edge configuration), log the mismatch and skip `fax_class1_
 *     progress` entirely for this flush, carrying a -1 sentinel through to
 *     the accumulator below.
 *   - Otherwise: poll `modem_recv_from_tty` for up to `host_rx_want`
 *     (clamped to 0x1000) bytes into `host_rx_buf`, gated on `host_rx_
 *     enable` and `host_rx_want` both being nonzero; call `fax_class1_
 *     progress`; store its `word8` result back into `host_rx_want`; and,
 *     when `host_rx_enable` is set and `word7` came back positive, forward
 *     `word7` bytes of `host_tx_buf` via `modem_send_to_tty`.  Dispatch the
 *     call's own `FAX_CLASS1_*` return (table below) to get this flush's
 *     forced status and any `host_rx_enable` transition.
 *   - When `rc_b != NULL`, resample `tx_pump_rate` into the `out_ring`
 *     position via `rc_b` (a no-op when `rc_a == NULL`, since `tx_pump_rate`
 *     was never the target `fax_class1_progress` wrote into).
 *   - A nonzero forced status becomes this OUTER iteration's `last_status`;
 *     `in_pending -= host_frame_samples`, `in_read_half` and `out_
 *     write_half` both toggle between 0 and `host_frame_samples`, and
 *     `out_produced += host_frame_samples`.
 *
 * Then, EVERY inner pass regardless of whether it flushed: drain the same
 * sample count back out of `out_ring` at `out_read_cursor` into the
 * current `out` position, and advance `out_read_cursor`/`out_produced`/
 * `out`/`count` to match.
 *
 * At each outer iteration's end, if that iteration's `last_status` is
 * nonzero, it becomes the function's own running return value -- so the
 * return is the LAST nonzero forced status seen across the whole call, and
 * an iteration that never flushed (or whose flushes were all
 * `FAX_CLASS1_NO_MESSAGE`) leaves the previous iteration's answer standing.
 *
 * THE DISPATCH TABLE, on `fax_class1_progress`'s own return (`class1.h`'s
 * `FAX_CLASS1_*`).  Every debug string below is gated on
 * `DSPLIB_DEBUG_ON()`; the two right columns are the flush's own forced
 * status and its effect on `host_rx_enable`:
 *
 *     status                          forced   host_rx_enable
 *     FAX_CLASS1_NO_MESSAGE       0    (same)   untouched
 *     FAX_CLASS1_OK                1    1        = 0
 *     FAX_CLASS1_ERROR             2    2        = 0
 *     FAX_CLASS1_OK_NO_CARRIER     3    1        = 0
 *     FAX_CLASS1_ERROR_NO_CARRIER  4    2        = 0
 *     FAX_CLASS1_ERROR_ON_HOOK     5    2        = 0
 *     FAX_CLASS1_CONNECT           6    3        = 1
 *     FAX_CLASS1_NO_CARRIER        7    4        = 0
 *     FAX_CLASS1_NO_CARRIER_NO_MESSAGE 8  0      = 0
 *     FAX_CLASS1_OTHER_CARRIER     9    0        = 0
 *     FAX_CLASS1_ACCEPT_RATE      10    0        untouched
 *     (anything else)                   2        untouched, logs
 *                                                 "fax: process: Unknown
 *                                                 status %d\n"
 *
 * `FAX_CLASS1_NO_MESSAGE` prints nothing and changes nothing -- its own
 * table entry lands mid-way into the shared "reload and continue" tail the
 * out-of-range default case also falls into, which is why both share no
 * dedicated code of their own.
 */
int
FAX_process(struct fax_ctx *ctx, const void *in, void *out, int count)
{
	int ret;

	ret = 0;
	while (count > 0) {
		int host_frame, chunk, remaining;
		int out_read_cursor_snap;
		const unsigned char *in_cur;
		unsigned char *out_cur;
		int last_status;

		host_frame = ctx->host_frame_samples;
		chunk = (count < host_frame) ? count : host_frame;

		in_cur = (const unsigned char *)in;
		out_cur = (unsigned char *)out;
		remaining = chunk;
		last_status = 0;
		out_read_cursor_snap = ctx->out_read_cursor;

		while (remaining > 0) {
			int cap, copy, room;
			short *in_ptr, *out_ptr;
			short *rx_buf, *tx_buf;
			int rx_count;
			int ebx;

			cap = 2 * host_frame;

			copy = host_frame;
			if (copy > remaining)
				copy = remaining;
			room = cap - ctx->in_write_cursor;
			if (copy > room)
				copy = room;
			room = cap - out_read_cursor_snap;
			if (copy > room)
				copy = room;

			sysdep_memcpy(&ctx->in_ring[ctx->in_write_cursor],
			    in_cur, copy * (int)sizeof(short));
			in_cur += copy * (int)sizeof(short);

			ctx->in_pending += copy;
			ctx->in_write_cursor =
			    (ctx->in_write_cursor + copy) % cap;

			if (ctx->in_pending >= host_frame) {
				in_ptr = &ctx->in_ring[ctx->in_read_half];
				out_ptr = &ctx->out_ring[ctx->out_write_half];

				if (ctx->rc_a != NULL) {
					int oc = 0xa0;

					RcFixed_Resample(ctx->rc_a, in_ptr,
					    host_frame, ctx->rx_resampled,
					    &oc);
					rx_buf = ctx->rx_resampled;
					rx_count = oc;
					tx_buf = ctx->tx_pump_rate;
				} else {
					rx_buf = in_ptr;
					rx_count = host_frame;
					tx_buf = out_ptr;
				}

				if (rx_count != 0xa0) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "fax: process: samples count %d != %d\n",
						    rx_count, 0xa0);
					ebx = -1;
				} else {
					int rxc, txc, w7, recv_n;

					rxc = 0xa0;
					txc = 0xa0;
					w7 = 0;
					recv_n = 0;

					if (ctx->host_rx_enable != 0 &&
					    ctx->host_rx_want != 0) {
						int want = ctx->host_rx_want;

						if (want > 0x1000)
							want = 0x1000;
						recv_n = modem_recv_from_tty(
						    ctx->modem,
						    ctx->host_rx_buf, want);
						if (recv_n > 0 &&
						    DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: snd: %d (%d)\n",
							    recv_n,
							    ctx->host_rx_want);
					}

					int status, forced;

					status = fax_class1_progress(
					    ctx->class1, rx_buf, tx_buf,
					    (int)(long)ctx->host_tx_buf,
					    (int)(long)ctx->host_rx_buf,
					    &rxc, &txc, &w7, &recv_n);
					ctx->host_rx_want = recv_n;

					if (ctx->host_rx_enable != 0 &&
					    w7 > 0) {
						modem_send_to_tty(ctx->modem,
						    ctx->host_tx_buf, w7);
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: rcv: %d (%d)\n",
							    recv_n, w7);
					}

					forced = 0;
					switch (status) {
					case FAX_CLASS1_NO_MESSAGE:
						break;
					case FAX_CLASS1_OK:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_OK\n");
						forced = 1;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_ERROR:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_ERROR\n");
						forced = 2;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_OK_NO_CARRIER:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_OK_NO_CARRIER\n");
						forced = 1;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_ERROR_NO_CARRIER:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_ERROR_NO_CARRIER\n");
						forced = 2;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_ERROR_ON_HOOK:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_ERROR_ON_HOOK\n");
						forced = 2;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_CONNECT:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_CONNECT\n");
						forced = 3;
						ctx->host_rx_enable = 1;
						break;
					case FAX_CLASS1_NO_CARRIER:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_NO_CARRIER\n");
						forced = 4;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_NO_CARRIER_NO_MESSAGE:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_NO_CARRIER_NO_MESSAGE\n");
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_OTHER_CARRIER:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_OTHER_CARRIER\n");
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_ACCEPT_RATE:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_ACCEPT_RATE\n");
						break;
					default:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: process: Unknown status %d\n",
							    status);
						forced = 2;
						break;
					}
					ebx = forced;
				}

				if (ctx->rc_b != NULL) {
					int oc2 = 0xa0;

					RcFixed_Resample(ctx->rc_b,
					    ctx->tx_pump_rate, host_frame,
					    out_ptr, &oc2);
				}

				if (ebx != 0)
					last_status = ebx;

				ctx->in_pending -= host_frame;
				ctx->in_read_half =
				    (ctx->in_read_half != 0) ? 0 : host_frame;
				ctx->out_produced += host_frame;
				ctx->out_write_half =
				    (ctx->out_write_half != 0) ? 0 :
				    host_frame;
			}

			sysdep_memcpy(out_cur,
			    &ctx->out_ring[ctx->out_read_cursor],
			    copy * (int)sizeof(short));
			out_cur += copy * (int)sizeof(short);
			ctx->out_produced -= copy;
			ctx->out_read_cursor =
			    (ctx->out_read_cursor + copy) % cap;

			remaining -= copy;
		}

		if (last_status != 0)
			ret = last_status;

		count -= chunk;
		in = (const unsigned char *)in + chunk;
		out = (unsigned char *)out + chunk;
	}

	return ret;
}
