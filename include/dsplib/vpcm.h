/*
 * vpcm.h -- the V.PCM datapump's root object, and its `.process`.
 *
 * `vpcm.c` is one translation unit of the original (`readelf -sW` puts
 * `vpcm_op`, `vpcm_create`, `vpcm_delete` and `vpcm_run` under its `STT_FILE`
 * entry) and it is the datapump `dp_vpcm_init` registers under ids 34, 90 and
 * 92.  Only `vpcm_run` is reconstructed so far; `vpcm_create`'s 969 bytes and
 * `vpcm_delete`'s 110 are not, and the fields below are named from what those
 * two write as read out of the disassembly rather than from a reconstruction.
 *
 * THE ROOT IS ITS OWN `struct dp`.  `vpcm_create` allocates 0xd258 bytes and
 * stores the block's own address back into it at +0x10:
 *
 *     3a42:  movl   $0xd258,(%esp)      sysdep_malloc
 *     3a76:  mov    %ebx,0x10(%ebx)     root->dp.dp_data = root
 *     3a93:  mov    %edi,(%ebx)         root->dp.id      = id
 *     3a90:  mov    %ecx,0x4(%ebx)      root->dp.modem   = modem
 *
 * so `dp` and `dp->dp_data` are the SAME pointer for this datapump, and
 * `t_v34link.c`'s `root[ep] = (char *)dp[ep]` with the V.34 object at
 * `root + 0x2c` is the same identification seen from the test's side.
 *
 * That matters when reading `vpcm_run`: the object reaches the modem handle
 * through `0x70(%esp)` (the `dp` argument) in the bit pipe and through
 * `%ebx` (the root) on the connect arm, which are two spellings of one value.
 * The reconstruction keeps both spellings where the object has them.
 */

#ifndef DSPLIB_VPCM_H
#define DSPLIB_VPCM_H

#include "dsplib/dp.h"
#include "dsplib/modem_params.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A sample queue: a count and the samples themselves, with the count in the
 * first four bytes.  Both of them are addressed as `base + 4 + 2*count` by
 * `vpcm_run` -- 0x3ea0 for the input one and 0x3eca for the output one.
 *
 * 52 IS THE MEASURED CAPACITY, not a guess: the input queue runs from
 * +0xd1e4 to the mute counter at +0xd250 and the output one from +0xd178 to
 * +0xd1e4, and both spans are 0x6c bytes -- four for the count and 104 for
 * 52 shorts.  It is exactly `vpcm_create`'s `max_frag` cap of 48 plus the
 * four samples the block quantisation can leave behind.
 */
struct vpcm_queue {
	int	count;			/* +0x00 samples currently held */
	short	buf[52];		/* +0x04                        */
};

/*
 * The V.34 object lives at root +0x2c and the PCM float buffers start at
 * +0xac78, so it is 0xac4c bytes.  It is `tagV34Object` -- the mangling of
 * `_Z16VPcmV34SetDelaysP12tagV34Object` names the type -- and this tree's
 * `struct v34_object` is a partial map of it.  Opaque here: `vpcm_run`
 * does not read a single field of it, it only passes its address to the five
 * `VPcmV34*` entry points below.
 */
#define VPCM_V34_BYTES		0xac4c

/*
 * TWO WORDS OF IT ARE NOT OPAQUE, AND THEY ARE `vpcm_create`'s.
 *
 * `vpcm_create` stores `VPCMXF_Create`'s answer at root +0x3574 and
 * `K56FLEX_Create`'s at root +0xac44, and `vpcm_delete` reads both back;
 * root +0x2c is where this block starts, so those are the V.34 object's own
 * +0x3548 and +0xac18.  `tools/whichfield.py struct v34_object 0x3548` and
 * `0xac18` both answer with a modelled `void *` -- this tree's partial map of
 * `tagV34Object` already carries them, as `p3548` and `pac18`.
 *
 * They are named here for what stores them.  `VPcmV34Create` is the other
 * witness and it is a strong one: at 0xaa7f and 0xaa92 it SAVES both words,
 * memsets the whole 0xac4c to zero, and puts both back (0xaaf9, 0xab15).  A
 * constructor that preserves exactly two words across a wholesale clear is
 * telling you those two were written before it ran and must survive it.
 *
 * The block is a struct rather than a byte array so the two are typed
 * members with `__builtin_offsetof` behind them.  It was
 * `unsigned char v34[VPCM_V34_BYTES]`, and `vpcm_run`'s five uses became
 * `&s->v34` where they were `s->v34`; nothing else changed.
 */
struct vpcm_v34 {
	unsigned char	opaque_0000[0x3548];	/* +0x0000              */
	void		*xf;			/* +0x3548 VPCMXF_Create */
	unsigned char	opaque_354c[0xac18 - 0x354c];
	void		*k56;			/* +0xac18 K56FLEX_Create */
	unsigned char	opaque_ac1c[VPCM_V34_BYTES - 0xac1c];
};

/* `nbits` is clamped to this, and both bit arrays hold exactly this many. */
#define VPCM_MAX_BITS		0x400u

/*
 * ---------------------------------------------------------------------------
 * `vpcm_create`'s literals.  Every one is an immediate in the disassembly at
 * the address named; none is derived from another.
 */

/* `cmp $0x2580,%esi; jne` at 0x3a1c -- an EQUALITY test, not a bound. */
#define VPCM_SRATE		9600

/* `cmpl $0x30,0x40(%esp); jg` at 0x3a37.  48 is the host's `MODEM_FRAG`. */
#define VPCM_MAX_FRAG		48

/* `cmp $0xdac0,%eax; jbe` at 0x3b65 -- UNSIGNED -- caps MDMPRM_MAX_RATE. */
#define VPCM_MAX_RATE_CAP	0xdac0u		/* 56000 */

/* Written into the runtime block's +0x38 and +0x3c as literals at 0x3b90. */
#define VPCM_PARAM_MIN_RATE	0x12c0		/*  4800 */
#define VPCM_PARAM_MAX_RATE	0x8340		/* 33600 */

/*
 * `and $0x210,%edi` at 0x3bd9.  528 samples of silence, and the mask survives
 * when the V.92 bit is CLEAR -- see the note above `vpcm_create` in
 * src/pump/v90/vpcm.c, because the `sbb` idiom reads the other way round.
 */
#define VPCM_MUTE_SAMPLES	0x210

/* `mov $0xf4,%ecx` at 0x3bf1: the largest hardware delay this will accept. */
#define VPCM_DELAY_CAP		0xf4

/* `cmp $0x180,%eax; jge` at 0x3d8c: the floor on the compensation. */
#define VPCM_EXTRADELAY_MIN	0x180

/*
 * How many blocks of an unchanged progress code 0 `vpcm_run` will sit through
 * before it calls the training a failure (`cmp $0xbb8,%esi` at 0x4274).
 */
#define VPCM_TRAIN_TIMEOUT	0xbb8

/*
 * The progress codes `VPcmV34Progress` returns, as the jump table at
 * .rodata+0x164 partitions them.  The table has seventeen entries and
 * `vpcm_run` sends anything above 0x10 to the same place as 2, 3 and 11-15,
 * which is "do nothing at all".
 */
#define VPCM_PROG_RESTART_P2	0	/* re-start phase II          */
#define VPCM_PROG_P2_DONE	1	/* phase II completed         */
#define VPCM_PROG_CONNECT_A	4	/* -> mode 1, CONNECTED       */
#define VPCM_PROG_CONNECT_B	5
#define VPCM_PROG_IDLE_A	6	/* -> mode 0                  */
#define VPCM_PROG_IDLE_B	7
#define VPCM_PROG_FAIL_A	8	/* -> mode -1, link error     */
#define VPCM_PROG_FAIL_B	9
#define VPCM_PROG_SAME_LINE	10	/* diagnostic only            */
#define VPCM_PROG_FAIL_C	16
#define VPCM_PROG_MAX		0x10

/* `mode`, the link state `vpcm_run` keeps at root +0x18. */
#define VPCM_MODE_ERROR		(-1)
#define VPCM_MODE_IDLE		0
#define VPCM_MODE_CONNECTED	1

/*
 * The three datapump ids the connect arm writes back into `dp.id`, which are
 * the same three `dp_vpcm_init` registers the table under.  0x22 is 34 and is
 * the fallback for every session type that is not one of the other two.
 */
#define VPCM_DP_V34		0x22
#define VPCM_DP_V90		0x5a
#define VPCM_DP_V92		0x5c

/*
 * The root object.  Sizes and offsets are `vpcm_create`'s 0xd258 allocation
 * and `vpcm_run`'s own accesses; `src/pump/v90/vpcm.c` asserts every one of
 * them with `__builtin_offsetof`.
 */
struct vpcm_root {
	struct dp	dp;		/* +0x00000 and root->dp.dp_data == root */
	int		status;		/* +0x00014 the last progress code seen  */
	int		mode;		/* +0x00018 VPCM_MODE_*                  */
	int		nbits;		/* +0x0001c bits to fetch next block     */
	int		stall;		/* +0x00020 blocks of unchanged code 0   */
	struct dsp_info	*info;		/* +0x00024 MDMPRM_DSPINFO               */
	struct _tagModemParameters *params;	/* +0x00028 MDMPRM_DPRUNTIME     */
	struct vpcm_v34	v34;		/* +0x0002c tagV34Object         */
	float		fin[160];	/* +0x0ac78 one block, as floats         */
	float		fout[160];	/* +0x0aef8                              */
	int		txbits[1024];	/* +0x0b178 bits going out on the line   */
	int		rxbits[1024];	/* +0x0c178 bits coming off it           */
	struct vpcm_queue outq;		/* +0x0d178                              */
	struct vpcm_queue inq;		/* +0x0d1e4                              */
	int		mute;		/* +0x0d250 samples still to be silenced */
	int		extradelay;	/* +0x0d254 the phase-II delay adjustment*/
};

/*
 * ---------------------------------------------------------------------------
 * The five entry points `vpcm_run` calls, and all five are reconstructed.
 *
 * All five are `extern "C"` in the object (no mangling on the relocation) and
 * every one is defined in this tree:
 * `VPcmV34GetCleanedSamples` and `VPcmV34GetCurrentSessionDP` in
 * `src/pump/v34/v34pcmif.c`, and `VPcmV34Progress` with the two rate getters
 * in `src/pump/v34/v34pcmmain.cpp` / `src/pump/v34/VPcmV34Main.cpp`.
 *
 * THE BLOB CALLS THEM DIRECTLY.  `vpcm_run` reaches each by an `R_386_PC32`
 * relocation with no null test and no indirect call -- `tools/dis.py
 * ref/slmodemd/dsplibs.o 0x3e40 0x44be` -- so they are plain prototypes here
 * and hard link requirements for any binary that links `vpcm.c`.  An earlier
 * `DSPLIB_VPCM_UNWRITTEN` weak-reference-plus-guard idiom was added apparatus
 * while they were unwritten; it is gone, because the object never had it.
 */
/**
 * @brief One block through V.34/V.90/V.92.
 *
 * @param obj    The V.34 object (`tagV34Object`, opaque here).
 * @param in     One block of input samples, as floats.
 * @param out    One block of output samples, as floats.
 * @param nin    Samples in @p in (and produced in @p out).
 * @param rxbits Out: bits recovered this block.
 * @param nrx    Out: how many bits were written to @p rxbits.
 * @param txbits In: bits to transmit this block.
 * @param nbits  In/out: bits available in @p txbits on entry; out, how many
 *               the modulator wants for the next block.
 * @return A `VPCM_PROG_*` progress code.
 */
int VPcmV34Progress(void *obj, float *in, float *out, int nin, int *rxbits,
		    int *nrx, int *txbits, int *nbits);

/** @brief The echo-cancelled input, for the host's data logger.
 *  @param obj The V.34 object. @param n Out: sample count (32 bytes' worth).
 *  @return The sample buffer. */
void *VPcmV34GetCleanedSamples(void *obj, int *n);

/** @brief Which of V.34, V.90 and V.92 the session settled on.
 *  @param obj The V.34 object. @return 34, 90 or 92. */
int VPcmV34GetCurrentSessionDP(void *obj);

/** @brief The agreed receive rate.
 *  @param obj The V.34 object. @return Bit rate in bit/s, for
 *  MDMPRM_RX_RATE. */
int VPcmV34GetCurrentRxBitRate(void *obj);

/** @brief The agreed transmit rate.
 *  @param obj The V.34 object. @return Bit rate in bit/s, for
 *  MDMPRM_TX_RATE. */
int VPcmV34GetCurrentTxBitRate(void *obj);

/*
 * ---------------------------------------------------------------------------
 * THE FOUR ENTRY POINTS ARE FILE-STATIC IN THE OBJECT.
 *
 * Every one of `vpcm_run`, `vpcm_create`, `vpcm_delete` and `vpcm_op` is
 * recorded LOCAL by `nm`, so all four are static in `src/pump/v90/vpcm.c` and
 * have no declarations here.  `vpcm_op` is reached the way the modem core
 * reaches it -- out of what `dp_vpcm_init` registered -- and its `.process`
 * IS `vpcm_run` directly (there is no `dp_wrapper` in front of this
 * datapump), so `harness_reg_ours.ops[0]->process` calls `vpcm_run` and
 * `->create`/`->destroy` call `vpcm_create`/`vpcm_delete`.  Finding F964 for
 * `count`, and `v22.h` for the same move on another datapump.
 */

/** @brief Register the V.PCM datapump under its three ids (V.34, V.90,
 *  V.92) with `modem_dp_register`. @return 0. */
int dp_vpcm_init(void);

/** @brief Counterpart of dp_vpcm_init(); the object's own body is empty. */
void dp_vpcm_exit(void);

/*
 * ---------------------------------------------------------------------------
 * The two `VPCMXF_` entry points `vpcm_create` and `vpcm_delete` call, and
 * the one `vpcm_delete` calls before them.  All three are `extern "C"` free
 * functions defined in C++ translation units, which is what makes the call
 * from this C one legal -- CLAUDE.md's three conditions, and the interop link
 * line already carries `$(CXXOBJ64)`.
 *
 * THE HANDLE IS A `VPcmFloModem *` and is spelled `void *` here, because this
 * header is included by C.  src/pump/v90/VPcmXfCreate.cpp declares the same
 * two with the real type; the two declarations never meet in one translation
 * unit and describe the same ABI.  `VPCMXF_SessionTermination` is declared in
 * src/pump/v90/VPcmXfTerm.cpp the same way and for the same reason, and is
 * repeated here because this is the caller.
 */
/** @brief Create the C++ V.PCM crossfeed helper. Defined in
 *  src/pump/v90/VPcmXfCreate.cpp; declared here `void *` because this header
 *  is included from C. @return A `VPcmFloModem *`, opaque here. */
void *VPCMXF_Create(int digitalSide, void *v34Object, void *dpRuntime,
		    unsigned int durationMs, int mode);

/** @brief Delete the object VPCMXF_Create() returned. */
void VPCMXF_Delete(void *self);

/** @brief Notify the crossfeed helper that the session is ending. Defined in
 *  src/pump/v90/VPcmXfTerm.cpp. */
void VPCMXF_SessionTermination(void *self);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_VPCM_H */
