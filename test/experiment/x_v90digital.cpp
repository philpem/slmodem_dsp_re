/*
 * x_v90digital.cpp -- BRINGING UP THE V.90 DIGITAL SIDE.  AN EXPERIMENT.
 *
 * ===========================================================================
 * THIS IS NOT A TEST AND IT IS NOT IN `test/unit/`
 * ===========================================================================
 *
 * Every binary in `test/unit/` answers "does our source behave as the blob's
 * does".  This one answers a different question, and it has no oracle for it:
 * `VPCMXF_Create`'s first argument selects the modem's SIDE, `vpcm_create`
 * passes a literal 0 on the only call site in 1.2 MB, and the other branch --
 * the DIGITAL side of a V.90 call, the modem that sends PCM downstream -- is
 * real code the vendor's object never constructs (findings F701, F702).  This
 * file calls `VPCMXF_Create(1, ...)` instead of `(0, ...)` and reports what
 * happens.
 *
 * BOTH SIDES OF EVERY PROBE ARE AVAILABLE, and that is what makes the answers
 * about the VENDOR rather than about us.  `VPCMXF_Create` is `extern "C"` in
 * the object, so `ref_VPCMXF_Create(1, ...)` is the vendor's own digital
 * constructor, callable through the ordinary `ref_` alias whatever the
 * shipped modem reached (finding F7000 overturning 702).  Where a probe runs
 * both, a symptom that appears on OUR side alone is a defect in the
 * reconstruction and a symptom that appears on BOTH is a property of the
 * vendor's branch.
 *
 * NOTHING HERE MAY BE READ AS A CONFORMANCE RESULT.  Milestone 4 loops a
 * digital instance back into an analogue one and both of them are our source,
 * so any defect the two share is invisible: a reversed bit order or a CRC
 * taken over the wrong extent would agree with itself perfectly.  It proves
 * function, never conformance.  The `ref_` arm narrows that to "any defect
 * the two share AND the blob shares", which is a much smaller hole but is
 * still not a wire test.
 *
 * ===========================================================================
 * WHY EVERY PROBE IS A `fork()`
 * ===========================================================================
 *
 * The expected outcome of several arms below is a fault.  `V90Mapper::reset`
 * runs `for (j = 0; j < constellationSize[i]; j++)` into a 128-byte row, and
 * `constellationSize` is `unsigned int` read out of a block nothing on this
 * side writes -- so the loop bound is whatever the allocation held.  A run
 * that died at the first such arm would report three of its arms as silence,
 * and finding F7573's rule is that an aborted check prints exactly what a
 * passing one prints.
 *
 * So each arm runs in a child, writes its measurements into a shared page,
 * and the PARENT does the reporting and the asserting.  A child that dies
 * leaves `done = 0` and the stage it reached, which is a positive reading and
 * not an absence.  `probe_fire_check` runs one child that returns cleanly and
 * one that faults deliberately, and refuses to go on unless the runner tells
 * them apart -- finding F134's ritual, applied to the instrument this file is
 * about to trust for every number it prints.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/v34fsk.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90CPUnPck.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90Mapper.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90Modulator.h"
#include "dsplib/V90Phase3Modulator.h"
#include "dsplib/V90Phase4Modulator.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Resampler.h"
#include "dsplib/tagV90AdditionalCPinfo.h"
#include "dsplib/v34shell.h"
#include "dsplib/VPcmFloModem.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void *VPCMXF_Create(int digitalSide, void *v34Object,
		    struct _tagModemParameters *dpRuntime,
		    unsigned int durationMs, int mode);
void VPCMXF_Delete(void *self);
void *ref_VPCMXF_Create(int digitalSide, void *v34Object,
			struct _tagModemParameters *dpRuntime,
			unsigned int durationMs, int mode);
void ref_VPCMXF_Delete(void *self);

/*
 * The blob's own copies of everything this file drives.  `setParamsInfoFrom-
 * CPUnPck` is `extern "C"` in the object -- which is exactly why finding
 * F7520's `nm | grep 16V90MappingParams` sweep could not see it -- so its
 * alias carries no mangling.
 */
void ref_setParamsInfoFromCPUnPck(V90MappingParams *params, V90CPUnPck *cp);

void ref_modem_progress(void *self, int *bits, unsigned int *nofBits,
			float *samples, unsigned int nofSymbols)
	asm("ref__ZN8V90Modem8progressEPiRjPfj");
void ref_mod_reset(void *self) asm("ref__ZN12V90Modulator5resetEv");
void ref_mod_enterp3(void *self) asm("ref__ZN12V90Modulator11enterPhase3Ev");
void ref_mod_enterp4(void *self) asm("ref__ZN12V90Modulator11enterPhase4Ev");
void ref_mod_exitri(void *self) asm("ref__ZN12V90Modulator6exitRiEv");
void ref_mod_enterdata(void *self)
	asm("ref__ZN12V90Modulator14enterDataPhaseEv");
void ref_bts_reset(void *self, V90MappingParams *mp, int pcm)
	asm("ref__ZN15V90BitsToSymbol5resetEP16V90MappingParams7PcmType");
unsigned int ref_bts_setblock(void *self, unsigned int n)
	asm("ref__ZN15V90BitsToSymbol19setSymbolsBlockSizeEj");
void ref_p4m_setmapping(void *self, V90MappingParams *mp)
	asm("ref__ZN18V90Phase4Modulator16setMappingParamsEP16V90MappingParams");
void ref_setPhaseIIinfo(void *self, int *info0, int rtd)
	asm("ref__ZN12VPcmFloModem14setPhaseIIinfoEPii");
int ref_v90RunDemodulator(void *self, float *in, unsigned int n, int *rxbits,
			  int *nrx)
	asm("ref__ZN12VPcmFloModem17v90RunDemodulatorEPfjPiS1_");
int ref_runPcmModem(void *self, float *in, float *out, unsigned int n,
		    int *rxbits, int *nrx, int *a, int *b)
	asm("ref__ZN12VPcmFloModem11runPcmModemEPfS0_jPiS1_S1_S1_");
}

/* ==================================================================== sizes */

#define FLO_SIZE	0x7f68
typedef char x_flo_is_0x7f68[(sizeof(VPcmFloModem) == FLO_SIZE) ? 1 : -1];

/*
 * `vpcm_create` computes `nsamples = max_frag * 1000 / srate` and hands it to
 * `VPCMXF_Create` as a DURATION IN MILLISECONDS, which scales it back by 9.6
 * on the analogue arm and by 8.0 on the digital one.  So the SAME duration
 * gives the two sides different buffers -- and `maxDataBuffer` is what
 * becomes `V90Modulator::nofSymbols`, the block size every `progress` call is
 * driven at.  10 ms is 80 symbols at V.90's 8 kHz downstream rate, which is
 * the block finding F7520's fixture drives, and 96 on the analogue side.
 */
#define DURATION_MS	10u
#define DIGITAL_SYMS	80		/* 10 * 8.0 + 0.5   */
#define ANALOG_SYMS	96		/* 10 * 9.6 + 0.5   */

#define NBITS		2048
#define NOUT		256

/* ============================================================ the shared page */

#define PROBE_V		48
#define PROBE_NOTE	160

struct probe_out {
	int		done;		/* the child reached its own end   */
	int		stage;		/* the last landmark it passed     */
	int		faulted;	/* a fault handler ran             */
	unsigned long	faultAddr;	/* and the address it faulted on   */
	long		v[PROBE_V];
	double		f[8];
	char		note[PROBE_NOTE];
};

struct probe_in {
	int		digitalSide;	/* `VPCMXF_Create`'s first argument */
	int		useBlob;	/* 0 ours, 1 the vendor's          */
	int		mode;		/* its fifth argument              */
	int		fill;		/* -1 leave the allocation's fill  */
	int		plausible;	/* poke a usable mapping set       */
	int		unpack;		/* call setParamsInfoFromCPUnPck   */
	int		blocks;		/* how many `progress` calls       */
	int		reach;		/* how far to drive the phases     */
	int		viaSetMapping;	/* enter the chain at its real head */
	int		which;		/* which driver the driver probe runs */
	int		forceDispatch;	/* past runPcmModem's early return  */
	int		faultOnPurpose;	/* the fire check                  */
};

/* How far `reach` drives.  Each is a superset of the one before it. */
#define REACH_NONE	0		/* construct and stop              */
#define REACH_SILENCE	1		/* progress in state 0             */
#define REACH_PHASE3	2		/* enterPhase3, then progress      */
#define REACH_MAPCHAIN	3		/* + the chain that reads the block*/
#define REACH_DATA	4		/* + state 3, the mapper runs      */

static struct probe_out *shared;

struct verdict {
	int		signo;	/* the signal that killed it, or 0         */
	int		status;	/* its exit status where it exited         */
	int		done;	/* it reached its own end                  */
	int		stage;	/* the last landmark it recorded           */
	unsigned long	addr;	/* the faulting address, where there is one */
	int		haveAddr;
};

/*
 * THE FAULTING ADDRESS, because "it died at stage 5" names a range of source
 * and not a site.  7623's central claim is that both V.PCM drivers fault on
 * `modem.demodulator->word_3c` with `demodulator` NULL, and the difference
 * between asserting that and measuring it is one `si_addr`: a fault at 0x3c
 * is a load of +0x3c through a null pointer and can be nothing else.  The
 * handler records and re-raises, so the parent still sees the signal and the
 * verdict table does not change shape.
 */
static void
fault_handler(int sig, siginfo_t *si, void *ctx)
{
	(void)ctx;
	if (shared != 0) {
		shared->faulted = 1;
		shared->faultAddr = (unsigned long)si->si_addr;
	}
	signal(sig, SIG_DFL);
	raise(sig);
}

static void
install_fault_handler(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof sa);
	sa.sa_sigaction = fault_handler;
	sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_RESETHAND;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, 0);
	sigaction(SIGBUS, &sa, 0);
	sigaction(SIGFPE, &sa, 0);
}

static const char *
signame(int s)
{
	switch (s) {
	case SIGSEGV:	return "SIGSEGV";
	case SIGBUS:	return "SIGBUS";
	case SIGFPE:	return "SIGFPE";
	case SIGILL:	return "SIGILL";
	case SIGABRT:	return "SIGABRT";
	case SIGKILL:	return "SIGKILL";
	default:	return "signal";
	}
}

/*
 * Run `fn` in a child and bring back both its measurements and how it died.
 * `fflush` before the fork, and `_exit` rather than `exit` in the child, so
 * no buffered line is written twice -- a duplicated report line is the same
 * class of lie as a missing one.
 */
static struct verdict
probe(void (*fn)(const struct probe_in *, struct probe_out *),
      const struct probe_in *in)
{
	struct verdict w;
	pid_t pid;
	int st = 0;

	memset(shared, 0, sizeof *shared);
	memset(&w, 0, sizeof w);

	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "experiment: fork failed\n");
		exit(2);
	}
	if (pid == 0) {
		install_fault_handler();
		fn(in, shared);
		fflush(stdout);
		_exit(0);
	}
	if (waitpid(pid, &st, 0) < 0) {
		fprintf(stderr, "experiment: waitpid failed\n");
		exit(2);
	}

	if (WIFSIGNALED(st))
		w.signo = WTERMSIG(st);
	else if (WIFEXITED(st))
		w.status = WEXITSTATUS(st);
	w.done = shared->done;
	w.stage = shared->stage;
	w.haveAddr = shared->faulted;
	w.addr = shared->faultAddr;
	return w;
}

static void
verdict_str(const struct verdict *w, char *buf, size_t n)
{
	if (w->signo != 0 && w->haveAddr)
		snprintf(buf, n, "DIED %s at 0x%lx, stage %d",
			 signame(w->signo), w->addr, w->stage);
	else if (w->signo != 0)
		snprintf(buf, n, "DIED %s at stage %d", signame(w->signo),
			 w->stage);
	else if (!w->done)
		snprintf(buf, n, "EXITED %d without finishing, stage %d",
			 w->status, w->stage);
	else
		snprintf(buf, n, "ran to the end");
}

/* ============================================================== the fixture */

static struct v34_object v34obj;
static unsigned char mparams[sizeof(struct _tagModemParameters)]
	__attribute__((aligned(8)));

#define MPARAMS		((struct _tagModemParameters *)(void *)mparams)

static int bits_in[NBITS];
static float out_f[NOUT];

/*
 * A mapping set the chain can survive, and its bounds are the object's rather
 * than this file's taste.  128 is `V90MAPPER_LEVELS`, the row length
 * `V90Mapper::reset` fills to; the LOWER bound is `ModulusEncoder::progress`,
 * which reads `bitsPerFrame - signBitsPerFrame` bits as one integer and
 * writes them out mixed-radix, so the six sizes must multiply past
 * 2**(bpf - 3) or the last digit indexes off the end of its row.  `shaperSR`
 * is 3, which divides six, so `V90BitsToSymbol::reset`'s `6 / shaperSR` is
 * exact.  This is finding F7520's fixture and the reasoning is its.
 */
static void
plausible_mapping(V90MappingParams *m, unsigned int bpf)
{
	unsigned int i, j;

	m->word_0 = bpf;
	for (i = 0; i < V90_CONSTELLATIONS; i++) {
		unsigned int len = 112u + 4u * i;

		if (len > V90_CONSTELLATION_MAX)
			len = V90_CONSTELLATION_MAX;
		m->constellationSize[i] = len;
		for (j = 0; j < V90_CONSTELLATION_MAX; j++) {
			m->constellation[i][j] =
			    (unsigned char)(0x11u + 3u * j + 7u * i);
			m->codecConstellation[i][j] =
			    (unsigned char)(0x21u + 5u * j + 3u * i);
		}
		m->distinctIndex[i] = (int)i;
	}
	m->word_61c = 1;
	m->shaperSR = 3;
	m->shaperId = 2;
	m->shaperA1 = 0.25f;
	m->shaperA2 = -0.125f;
	m->shaperB1 = 0.5f;
	m->shaperB2 = 0.0625f;
}

/*
 * A `V90CPUnPck` -- the received CP message, in the shape the ONE reader of
 * that layout expects.  All-ones bitmaps: 128 possible entries into a
 * 128-byte table, which finding F7570 measured as exactly filling one
 * constellation and unable to overrun it.  `info->word_04` non-zero picks the
 * `+ 0x14` arm of `setDataBitRate`, so `word_0` comes out at rate + 20.
 */
static struct tagV90AdditionalCPinfo unpck_info;

static void
plausible_cpunpck(struct V90CPUnPck *cp, unsigned int wantBpf)
{
	int i, j;

	memset(cp, 0, sizeof *cp);
	memset(&unpck_info, 0, sizeof unpck_info);
	unpck_info.word_04 = 1;
	cp->info = &unpck_info;
	cp->dataBitRate = wantBpf - 0x14u;

	cp->shaperSR = 3;
	cp->shaperId = 2;
	cp->shaperA1 = 0.25f;
	cp->shaperA2 = -0.125f;
	cp->shaperB1 = 0.5f;
	cp->shaperB2 = 0.0625f;

	cp->codecConstellationPresent = 1;
	for (i = 0; i < V90_CPUNPCK_CONSTELS; i++) {
		cp->distinctIndex[i] = (unsigned char)i;
		for (j = 0; j < V90_CPUNPCK_MASK_WORDS; j++) {
			cp->constellationMask[i][j] = (short)0xffff;
			cp->codecConstellationMask[i][j] = (short)0xffff;
		}
	}
}

/* ==================================================== the probe bodies */

#define STAGE(o, n)	((o)->stage = (n))

/*
 * Everything a probe does before it chooses an arm.  Returns the modem or 0.
 * The V.34 object and the runtime parameter block are ZEROED rather than
 * seeded, for t_v90modprog.cpp's reason: every field of `V90Parameters` is
 * derived from the second, and a seeded one gives a signalling NaN about one
 * word in 250 and an allocation length of two billion rather more often.
 */
static VPcmFloModem *
build(const struct probe_in *in)
{
	memset(&v34obj, 0, sizeof v34obj);
	memset(mparams, 0, sizeof mparams);
	harness_alloc_reset();
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	if (in->useBlob)
		return (VPcmFloModem *)ref_VPCMXF_Create(in->digitalSide,
		    &v34obj, MPARAMS, DURATION_MS, in->mode);
	return (VPcmFloModem *)VPCMXF_Create(in->digitalSide, &v34obj, MPARAMS,
					     DURATION_MS, in->mode);
}

static void
destroy(const struct probe_in *in, VPcmFloModem *flo)
{
	if (in->useBlob)
		ref_VPCMXF_Delete(flo);
	else
		VPCMXF_Delete(flo);
}

/*
 * MILESTONE 1 -- does a digital-side modem CONSTRUCT?
 *
 * Finding F701 says the plumbing that selects the modulator arm is "present
 * and correct" and that the value is inverted on the way in: `sete %al` puts
 * `(digitalSide == 0)` into the `V90ModemSide` slot, so a NON-zero first
 * argument is the DIGITAL side and gives `V90ModemSide` 0, which
 * `V90SessionFlag.h` established selects the modulator at +0x00.  This is the
 * first execution of that claim.
 */
static void
probe_construct(const struct probe_in *in, struct probe_out *o)
{
	VPcmFloModem *flo;
	V90Modem *m;

	STAGE(o, 1);
	flo = build(in);
	STAGE(o, 2);

	o->v[0] = flo != 0;
	if (flo == 0) {
		o->done = 1;
		return;
	}

	m = &flo->modem;
	o->v[1] = (long)m->side;
	o->v[2] = m->modulator != 0;
	o->v[3] = m->demodulator != 0;
	o->v[4] = m->modulator ? (long)m->modulator->nofSymbols : -1;
	o->v[5] = m->modulator ? (long)m->modulator->bitsToSymbol->nofSymbols
			       : -1;
	o->v[6] = m->modulator ? (long)m->modulator->state : -1;
	o->v[7] = (long)harness_alloc.allocs;
	o->v[8] = (long)harness_alloc.bytes;
	o->v[9] = m->params != 0;

	/*
	 * The mapping block AS THE CONSTRUCTOR LEFT IT.  Finding F7520's claim
	 * is that `V90Modem::V90Modem` does not construct it, so it holds
	 * whatever the allocation held -- which the harness's `sysdep_malloc`
	 * makes a fixed 0xa5 pattern deliberately, so that a field nobody
	 * wrote is obviously wrong rather than plausibly zero.
	 */
	o->v[10] = (long)m->mappingParams.word_0;
	o->v[11] = (long)m->mappingParams.constellationSize[0];
	o->v[12] = (long)m->mappingParams.shaperSR;
	o->v[13] = (long)m->mappingParamsAlt.word_0;

	STAGE(o, 3);
	destroy(in, flo);
	STAGE(o, 4);
	o->v[14] = (long)harness_alloc.live;
	o->v[15] = (long)harness_alloc.bad_free;
	o->done = 1;
}

/*
 * Bring the mapping block to the state this arm wants, then run whichever
 * part of the chain the arm asks for.  Shared by milestones 2 and 3 because
 * they are the same drive with different starting blocks -- which is the
 * point: the ONLY difference between "it runs" and "it does not" is what is
 * in a 0x650-byte structure nothing on this side writes.
 */
static void
prepare_block(const struct probe_in *in, V90Modem *m)
{
	struct V90CPUnPck cp;

	if (in->fill >= 0) {
		memset(&m->mappingParams, in->fill, sizeof m->mappingParams);
		memset(&m->mappingParamsAlt, in->fill,
		       sizeof m->mappingParamsAlt);
	}
	if (in->plausible) {
		plausible_mapping(&m->mappingParams, 42u);
		plausible_mapping(&m->mappingParamsAlt, 42u);
	}
	if (in->unpack) {
		plausible_cpunpck(&cp, 42u);
		/*
		 * THE HARNESS SUPPLIES THE CALL THE OBJECT DOES NOT HAVE.
		 * `setParamsInfoFromCPUnPck` writes essentially the whole of
		 * `V90MappingParams` (finding F7570) and has ZERO relocations
		 * naming it in 1.2 MB.  Calling it from here is the question
		 * "would a caller have been enough", asked without adding one
		 * to `src/`.
		 */
		if (in->useBlob) {
			ref_setParamsInfoFromCPUnPck(&m->mappingParams, &cp);
			ref_setParamsInfoFromCPUnPck(&m->mappingParamsAlt,
						     &cp);
		} else {
			setParamsInfoFromCPUnPck(&m->mappingParams, &cp);
			setParamsInfoFromCPUnPck(&m->mappingParamsAlt, &cp);
		}
	}
}

/*
 * MILESTONES 2 AND 3 -- does it RUN, and where does the mapping block bite?
 *
 * `V90Modem::progress` is the whole `side` switch and forwards to
 * `V90Modulator::progress` on the digital arm.  That function's four states
 * are silence, phase 3, phase 4 and the data phase; the data phase is the one
 * that reaches `V90Mapper::process`, and the only route by which the mapper
 * ever reads `V90MappingParams` is
 *
 *     V90Phase4Modulator::setMappingParams -> V90BitsToSymbol::reset
 *                                          -> V90Mapper::reset
 *
 * which nothing inside `V90Modulator` calls: `exitRi` is the class's only
 * caller of `setMappingParams` and `exitRi` is an entry point for a driver
 * above this class.  The `REACH_MAPCHAIN` arm below therefore CALLS
 * `V90BitsToSymbol::reset` itself, standing in for that missing driver, and
 * that substitution is the experiment rather than a convenience.
 */
static void
probe_run(const struct probe_in *in, struct probe_out *o)
{
	VPcmFloModem *flo;
	V90Modem *m;
	V90Modulator *mod;
	unsigned int nofBits = 0;
	unsigned int n;
	int b, i;
	long nonzero = 0, nan = 0;
	double lo = 1e300, hi = -1e300;

	STAGE(o, 1);
	flo = build(in);
	if (flo == 0) {
		o->done = 1;
		o->v[0] = 0;
		return;
	}
	o->v[0] = 1;
	m = &flo->modem;
	mod = m->modulator;
	o->v[1] = mod != 0;
	if (mod == 0) {
		o->done = 1;
		return;
	}
	n = mod->nofSymbols;
	if (n > NOUT)
		n = NOUT;
	o->v[2] = (long)n;

	STAGE(o, 2);
	prepare_block(in, m);
	o->v[3] = (long)m->mappingParamsAlt.word_0;
	o->v[4] = (long)m->mappingParamsAlt.constellationSize[0];
	o->v[5] = (long)m->mappingParamsAlt.shaperSR;

	/*
	 * The phase 2 record, which `enterPhase3` and the phase 4 reset both
	 * read.  A fresh `V90Phase2Info` leaves these at the constructor's
	 * values; a real session fills them from the INFO exchange.
	 */
	m->phase2Info->pcmType = PCM_TYPE_MU_LAW;
	m->phase2Info->Uinfo = 0x40;
	m->phase2Info->rtd = 7;
	m->params->DEBUG_DIGITAL_MODEM_INITIATE_RRN = 0;
	m->params->DEBUG_DIGITAL_MODEM_INITIATE_RRN_TIME = 0;

	STAGE(o, 3);
	if (in->useBlob)
		ref_mod_reset(mod);
	else
		mod->reset();
	o->v[6] = mod->state;

	if (in->reach >= REACH_PHASE3) {
		STAGE(o, 4);
		if (in->useBlob)
			ref_mod_enterp3(mod);
		else
			mod->enterPhase3();
		o->v[7] = mod->state;
	}

	if (in->reach >= REACH_MAPCHAIN) {
		/*
		 * STAGE 5 IS THE ONE THAT READS THE BLOCK.  Everything above
		 * it runs whatever the block holds, because nothing above it
		 * looks at the block.
		 */
		STAGE(o, 5);
		if (in->viaSetMapping) {
			/*
			 * THE CHAIN'S REAL HEAD.  7520 names it as
			 * `V90Phase4Modulator::setMappingParams` ->
			 * `V90BitsToSymbol::reset` -> `V90Mapper::reset`, and
			 * entering at the second link leaves two differences
			 * a caveat would have had to carry instead: the
			 * `PcmType` comes from the phase 4 modulator's own
			 * field rather than from the harness, and the head
			 * sets the block size to 1 before anything else does.
			 * Both are driven here so the two entries can be
			 * compared rather than assumed equivalent.
			 */
			mod->phase4Modulator->pcmType = PCM_TYPE_MU_LAW;
			if (in->useBlob)
				ref_p4m_setmapping(mod->phase4Modulator,
						   &m->mappingParamsAlt);
			else
				mod->phase4Modulator->setMappingParams(
				    &m->mappingParamsAlt);
		} else if (in->useBlob) {
			ref_bts_reset(mod->bitsToSymbol, &m->mappingParamsAlt,
				      PCM_TYPE_MU_LAW);
		} else {
			mod->bitsToSymbol->reset(&m->mappingParamsAlt,
						 PCM_TYPE_MU_LAW);
		}
		STAGE(o, 6);
		o->v[8] = (long)mod->bitsToSymbol->mapper->bitsPerFrame;
		o->v[9] = (long)mod->bitsToSymbol->bitsPerFrame;
		o->v[10] = (long)mod->bitsToSymbol->mapper->signBitsPerFrame;
		o->v[11] = (long)mod->bitsToSymbol->mapper->signBitGroups;
		o->v[12] = (long)mod->bitsToSymbol->mapper->signBitGroupSize;
	}

	if (in->reach >= REACH_DATA) {
		STAGE(o, 7);
		if (in->useBlob)
			ref_bts_setblock(mod->bitsToSymbol, n);
		else
			mod->bitsToSymbol->setSymbolsBlockSize(n);
		/*
		 * The data phase is entered here rather than reached, and the
		 * reason is the same missing driver: `V90Modulator::progress`
		 * moves state 2 -> 3 only when the phase 4 modulator raises
		 * event 7, and what raises it is a training sequence a driver
		 * would have been clocking for thousands of symbols.
		 */
		mod->state = 3;
		nofBits = mod->bitsToSymbol->nofBitsForNextTime();
		o->v[13] = (long)nofBits;
		if (nofBits > NBITS)
			nofBits = NBITS;
	}

	for (i = 0; i < NBITS; i++)
		bits_in[i] = (i * 2654435761u) & 1;

	STAGE(o, 8);
	for (b = 0; b < in->blocks; b++) {
		unsigned int nb = nofBits;

		/*
		 * THE BITS ASKED FOR, PER BLOCK, AND THE SEQUENCE RATHER THAN
		 * ONE SAMPLE OF IT.  `V90BitsToSymbol::nofBitsForNextTime`
		 * hands out whole FRAMES, and an 80-symbol block is 13 1/3
		 * six-symbol frames -- so a single reading cannot be turned
		 * into a bit rate, and one that was would be out by a whole
		 * frame.  The MEAN over the run is the rate; the spread is
		 * the converter's frame-boundary rounding.
		 */
		if (b < 8)
			o->v[32 + b] = (long)nofBits;
		o->v[44] += (long)nofBits;
		o->v[45]++;

		memset(out_f, 0, sizeof out_f);
		if (in->useBlob)
			ref_modem_progress(m, bits_in, &nb, out_f, n);
		else
			m->progress(bits_in, nb, out_f, n);

		o->v[16 + (b & 7)] = (long)nb;
		for (i = 0; i < (int)n; i++) {
			float s = out_f[i];

			if (s != 0.0f)
				nonzero++;
			if (diff_isnan_f(s))
				nan++;
			if (s < lo)
				lo = s;
			if (s > hi)
				hi = s;
		}
		if (in->reach >= REACH_DATA)
			nofBits = mod->bitsToSymbol->nofBitsForNextTime();
	}
	STAGE(o, 9);

	o->v[24] = nonzero;
	o->v[25] = nan;
	o->v[26] = mod->state;
	o->v[27] = (long)mod->eventCode;
	o->v[28] = (long)mod->symbolCount;
	o->v[29] = (long)mod->phase3Modulator->eventCode;
	o->v[30] = (long)mod->phase4Modulator->eventCode;
	o->f[0] = lo;
	o->f[1] = hi;

	STAGE(o, 10);
	destroy(in, flo);
	o->v[31] = (long)harness_alloc.live;
	STAGE(o, 11);
	o->done = 1;
}

/*
 * THE DRIVER PROBE.  `VPcmFloModem::runPcmModem` and `v90RunDemodulator` are
 * the two entry points a V.PCM session runs through, and both dispatch on
 * `this->modem.demodulator->word_3c` -- a field of an object that exists only
 * on the ANALOGUE side.  Finding F7581 argued from the two `.rodata` jump
 * tables that no arm of the V.90 driver could hold the missing unpacker's
 * call; this asks the blunter question one level up, which is whether either
 * driver can be entered at all on a digital instance.
 */
static void
probe_driver(const struct probe_in *in, struct probe_out *o)
{
	VPcmFloModem *flo;
	static float sig[NOUT], outbuf[NOUT];
	static int rxbits[NOUT];
	int nrx = 0, a = 0, bb = 0;

	STAGE(o, 1);
	flo = build(in);
	if (flo == 0) {
		o->done = 1;
		return;
	}
	o->v[0] = flo->modem.demodulator != 0;
	o->v[1] = flo->modem.modulator != 0;
	memset(sig, 0, sizeof sig);
	memset(outbuf, 0, sizeof outbuf);

	/*
	 * `runPcmModem` RETURNS BEFORE THE DISPATCH ON A FRESH OBJECT.  Its
	 * fourth statement is `if (info0Layout == 0) return ret;` at
	 * .text+0xe470, and a constructed modem has that field at zero, so
	 * "it did not fault" on a virgin object says nothing about the
	 * demodulator dereference twelve instructions later.  This arm sets
	 * the field so the function reaches its own second jump table, which
	 * is the load 7581 is about.
	 */
	if (in->forceDispatch)
		flo->info0Layout = 1;
	o->v[3] = flo->info0Layout;

	STAGE(o, 2);
	if (in->which == 1) {
		if (in->useBlob)
			o->v[2] = ref_v90RunDemodulator(flo, sig, 48, rxbits,
							&nrx);
		else
			o->v[2] = flo->v90RunDemodulator(sig, 48, rxbits,
							 &nrx);
	} else {
		if (in->useBlob)
			o->v[2] = ref_runPcmModem(flo, sig, outbuf, 48,
						  rxbits, &nrx, &a, &bb);
		else
			o->v[2] = flo->runPcmModem(sig, outbuf, 48, rxbits,
						   &nrx, &a, &bb);
	}
	STAGE(o, 3);
	o->done = 1;
}

/*
 * MILESTONE 4 -- a digital instance against an analogue one, both ours.
 *
 * `V90Modulator::progress` closes with `out[i] = symbolBuf[i]`, so the float
 * buffer it fills holds PCM CODEWORD INDICES and not line samples: the
 * digital modem chooses codewords and the network is assumed to deliver them
 * unaltered (finding F7000's transport note).  The analogue side's
 * `V90Demodulator` expects the client's internal rate, which `VPCMXF_Create`
 * itself says is 9600 against the digital side's 8000 -- 96 symbols a block
 * against 80.  So this feed is not a channel and is not claimed to be one;
 * what it measures is whether the two halves can be clocked against each
 * other at all and what the receiving side does with what the transmitter
 * emits.
 */
static void
probe_loopback(const struct probe_in *in, struct probe_out *o)
{
	static struct v34_object v34d, v34a;
	static unsigned char mpd[sizeof(struct _tagModemParameters)]
		__attribute__((aligned(8)));
	static unsigned char mpa[sizeof(struct _tagModemParameters)]
		__attribute__((aligned(8)));
	static float link[NOUT];
	static int rxbits[NOUT];
	static int info0[64];
	VPcmFloModem *dig, *ana;
	V90Modulator *mod;
	unsigned int nofBits = 0, nd;
	int b, i, nrx = 0;
	long moved = 0, nonzero = 0;

	memset(&v34d, 0, sizeof v34d);
	memset(&v34a, 0, sizeof v34a);
	memset(mpd, 0, sizeof mpd);
	memset(mpa, 0, sizeof mpa);
	memset(info0, 0, sizeof info0);
	harness_alloc_reset();
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	STAGE(o, 1);
	dig = (VPcmFloModem *)VPCMXF_Create(1, &v34d,
	    (struct _tagModemParameters *)mpd, DURATION_MS, 3);
	ana = (VPcmFloModem *)VPCMXF_Create(0, &v34a,
	    (struct _tagModemParameters *)mpa, DURATION_MS, 3);
	o->v[0] = dig != 0;
	o->v[1] = ana != 0;
	if (dig == 0 || ana == 0) {
		o->done = 1;
		return;
	}
	o->v[2] = dig->modem.modulator != 0;
	o->v[3] = dig->modem.demodulator != 0;
	o->v[4] = ana->modem.modulator != 0;
	o->v[5] = ana->modem.demodulator != 0;

	STAGE(o, 2);
	mod = dig->modem.modulator;
	plausible_mapping(&dig->modem.mappingParams, 42u);
	plausible_mapping(&dig->modem.mappingParamsAlt, 42u);
	dig->modem.phase2Info->pcmType = PCM_TYPE_MU_LAW;
	dig->modem.phase2Info->Uinfo = 0x40;
	dig->modem.phase2Info->rtd = 7;
	dig->modem.params->DEBUG_DIGITAL_MODEM_INITIATE_RRN = 0;
	mod->reset();
	mod->enterPhase3();
	nd = mod->nofSymbols;
	if (nd > NOUT)
		nd = NOUT;
	o->v[6] = (long)nd;

	/*
	 * The analogue side's Phase 2 record has to be filled before its
	 * demodulator is driven -- `V90PreFilter::selectFilter` reaches
	 * `L2` through a pointer a freshly constructed demodulator has not
	 * set (t_v90rundemod.cpp's D561 note).
	 */
	STAGE(o, 3);
	ana->setPhaseIIinfo(info0, 7);
	o->v[7] = (long)ana->modem.demodulator->word_3c;

	STAGE(o, 4);
	for (b = 0; b < in->blocks; b++) {
		unsigned int nb = nofBits;
		int before, after;

		memset(link, 0, sizeof link);
		dig->modem.progress(bits_in, nb, link, nd);
		for (i = 0; i < (int)nd; i++)
			if (link[i] != 0.0f)
				nonzero++;

		/*
		 * THE NEGATIVE CONTROL, and the loopback's conclusion rests
		 * entirely on it.  `silence` runs the identical eight blocks
		 * with the link buffer zeroed AFTER the modulator filled it,
		 * so the demodulator sees a silent input and everything else
		 * about the run is the same.  If its event word moves the
		 * same number of times either way, what is moving is the
		 * demodulator's own state machine and NOT a reaction to what
		 * the transmitter emitted -- which is exactly the reading a
		 * count with no control invites.
		 */
		if (in->faultOnPurpose)
			memset(link, 0, sizeof link);

		o->v[8 + (b & 7)] = (long)mod->eventCode;

		before = ana->modem.demodulator->word_3c;
		nrx = 0;
		ana->v90RunDemodulator(link, nd, rxbits, &nrx);
		after = ana->modem.demodulator->word_3c;
		if (after != before)
			moved++;
		o->v[16 + (b & 7)] = after;
		o->v[24] = nrx;
	}
	STAGE(o, 5);

	o->v[25] = moved;
	o->v[26] = nonzero;
	o->v[27] = mod->state;
	o->v[28] = (long)mod->eventCode;
	o->v[29] = ana->modem.demodulator->word_3c;
	o->v[30] = ana->modem.demodulator->inPhase3;

	VPCMXF_Delete(dig);
	VPCMXF_Delete(ana);
	o->v[31] = (long)harness_alloc.live;
	STAGE(o, 6);
	o->done = 1;
}

/*
 * The event-code vocabulary.  Finding F7581 measured the two drivers' jump
 * tables off `.rodata`: `v90RunDemodulator` covers 0x00..0x2b and
 * `runPcmModem` covers 0x00..0x35, on the SAME field, with two different
 * vocabularies.  What that finding could not do is say which codes the
 * digital MODULATOR actually raises, because nothing had ever run it.  This
 * collects them.
 */
static void
probe_eventcodes(const struct probe_in *in, struct probe_out *o)
{
	VPcmFloModem *flo;
	V90Modem *m;
	V90Modulator *mod;
	unsigned int nofBits = 0, n;
	int b, i;

	STAGE(o, 1);
	flo = build(in);
	if (flo == 0) {
		o->done = 1;
		return;
	}
	m = &flo->modem;
	mod = m->modulator;
	if (mod == 0) {
		o->done = 1;
		return;
	}
	plausible_mapping(&m->mappingParams, 42u);
	plausible_mapping(&m->mappingParamsAlt, 42u);
	m->phase2Info->pcmType = PCM_TYPE_MU_LAW;
	m->phase2Info->Uinfo = 0x40;
	m->phase2Info->rtd = 7;
	m->params->DEBUG_DIGITAL_MODEM_INITIATE_RRN = 0;
	n = mod->nofSymbols;
	if (n > NOUT)
		n = NOUT;

	STAGE(o, 2);
	mod->reset();
	mod->enterPhase3();

	for (b = 0; b < in->blocks; b++) {
		unsigned int nb = nofBits;

		m->progress(bits_in, nb, out_f, n);
		if (mod->eventCode < 40)
			o->v[mod->eventCode]++;
		o->v[40]++;
	}
	STAGE(o, 3);
	o->v[41] = mod->state;
	destroy(in, flo);
	for (i = 0; i < 40; i++)
		if (o->v[i] != 0 && i > (int)o->v[42])
			o->v[42] = i;
	o->done = 1;
}

/* ============================================== the fire check for the runner */

static void
probe_clean(const struct probe_in *in, struct probe_out *o)
{
	(void)in;
	STAGE(o, 7);
	o->v[0] = 0x1234;
	o->done = 1;
}

static void
probe_fault(const struct probe_in *in, struct probe_out *o)
{
	volatile int *p = (volatile int *)0;

	(void)in;
	STAGE(o, 3);
	o->v[0] = *p;		/* deliberate */
	o->done = 1;
}

/* ==================================================================== report */

static int nfail;

static void
must(int ok, const char *what)
{
	printf("      %-62s %s\n", what, ok ? "yes" : "NO");
	if (!ok)
		nfail++;
}

/*
 * One data-phase run's numbers.  The RANGE is printed beside the non-zero
 * count deliberately: a count alone cannot separate emitted symbols from
 * `symbolBuf`'s untouched 0xa5a5 fill, which is -23131 and is also non-zero.
 * The mean bits a block is printed as a RATE because one block cannot be:
 * the converter hands out whole six-symbol frames and 80 symbols is 13 1/3
 * of them.
 */
static void
report_run(const struct probe_in *in)
{
	int b, n = (int)shared->v[45];

	printf("        mapper bitsPerFrame=%ld signBits=%ld groups=%ld "
	       "groupSize=%ld\n", shared->v[8], shared->v[10], shared->v[11],
	       shared->v[12]);
	printf("        bits asked for, per block:");
	for (b = 0; b < in->blocks && b < 8; b++)
		printf(" %ld", shared->v[32 + b]);
	if (n > 0)
		printf("   mean %.1f over %d block(s) = %.0f bit/s\n",
		       (double)shared->v[44] / n, n,
		       (double)shared->v[44] / n * 8000.0
		       / (double)shared->v[2]);
	else
		printf("   (no block ran)\n");
	printf("        %ld non-zero samples, range [%g, %g], %ld NaN\n",
	       shared->v[24], shared->f[0], shared->f[1], shared->v[25]);
}

static void
show(const struct verdict *w)
{
	char buf[96];

	verdict_str(w, buf, sizeof buf);
	printf("      -> %s\n", buf);
}

int
main(void)
{
	struct probe_in in;
	struct verdict w, wb;
	int i;

	shared = (struct probe_out *)mmap(0, sizeof(struct probe_out),
					  PROT_READ | PROT_WRITE,
					  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (shared == MAP_FAILED) {
		fprintf(stderr, "experiment: mmap failed\n");
		return 2;
	}

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	for (i = 0; i < NBITS; i++)
		bits_in[i] = (i * 2654435761u) & 1;

	printf("\n=== V.90 DIGITAL TERMINATION: an experiment, not a "
	       "reconstruction ===\n");
	printf("    sizeof(VPcmFloModem) = 0x%x, allocation fill = 0x%02x\n",
	       (unsigned)sizeof(VPcmFloModem), HARNESS_MALLOC_FILL);

	/* ------------------------------------------------ the fire check */
	printf("\n[0] THE PROBE RUNNER ITSELF\n");
	memset(&in, 0, sizeof in);
	w = probe(probe_clean, &in);
	printf("    a child that returns normally:\n");
	show(&w);
	must(w.done == 1 && w.signo == 0 && w.stage == 7,
	     "reported as finished, at the stage it recorded");
	wb = probe(probe_fault, &in);
	printf("    a child that dereferences NULL on purpose:\n");
	show(&wb);
	must(wb.signo == SIGSEGV && wb.done == 0 && wb.stage == 3,
	     "reported as DIED, with the last stage it reached");
	must(wb.haveAddr && wb.addr == 0,
	     "the faulting address came back, and is 0 for a null read");
	must(!(w.done == wb.done && w.signo == wb.signo),
	     "the two are distinguishable (an aborted probe is not a pass)");

	/* --------------------------------------------------- milestone 1 */
	printf("\n[1] DOES IT CONSTRUCT?  VPCMXF_Create(1, ...)\n");
	for (i = 0; i < 4; i++) {
		int digital = (i & 1) == 0;
		int blob = (i & 2) != 0;

		memset(&in, 0, sizeof in);
		in.digitalSide = digital ? 1 : 0;
		in.useBlob = blob;
		in.mode = 3;
		w = probe(probe_construct, &in);
		printf("    %-8s side, %-6s: ", digital ? "DIGITAL" : "analog",
		       blob ? "BLOB" : "ours");
		if (w.signo || !w.done) {
			char buf[96];

			verdict_str(&w, buf, sizeof buf);
			printf("%s\n", buf);
			nfail++;
			continue;
		}
		printf("built=%ld V90ModemSide=%ld modulator=%ld "
		       "demodulator=%ld\n",
		       shared->v[0], shared->v[1], shared->v[2],
		       shared->v[3]);
		printf("              nofSymbols=%ld btsSymbols=%ld "
		       "allocs=%ld bytes=%ld live-after-delete=%ld\n",
		       shared->v[4], shared->v[5], shared->v[7],
		       shared->v[8], shared->v[14]);
		printf("              mapping block as constructed: "
		       "word_0=0x%lx size[0]=0x%lx shaperSR=0x%lx\n",
		       (unsigned long)shared->v[10],
		       (unsigned long)shared->v[11],
		       (unsigned long)shared->v[12]);

		must(shared->v[0] == 1, "the modem was allocated");
		if (digital) {
			must(shared->v[1] == 0, "V90ModemSide is 0");
			must(shared->v[2] == 1, "the MODULATOR exists");
			must(shared->v[3] == 0, "the demodulator is NULL");
			must(shared->v[4] == DIGITAL_SYMS,
			     "nofSymbols is 80, the 8 kHz scaling");
		} else {
			must(shared->v[1] == 1, "V90ModemSide is 1");
			must(shared->v[2] == 0, "the modulator is NULL");
			must(shared->v[3] == 1, "the DEMODULATOR exists");
			must(shared->v[4] == -1,
			     "no modulator to report nofSymbols");
		}
		must(shared->v[14] == 0, "nothing left allocated after delete");
		must(shared->v[15] == 0, "no bad free");
	}

	/* --------------------------------------------------- milestone 2 */
	printf("\n[2] DOES IT RUN?  V90Modem::progress on the digital arm\n");
	{
		static const struct {
			int reach;
			const char *what;
		} steps[] = {
		    { REACH_SILENCE,  "state 0, silence" },
		    { REACH_PHASE3,   "state 1, phase 3 after enterPhase3()" },
		};
		unsigned s;
		int ran = 0;

		for (s = 0; s < sizeof steps / sizeof steps[0]; s++) {
			int side;

			for (side = 0; side < 2; side++) {
				memset(&in, 0, sizeof in);
				in.digitalSide = 1;
				in.useBlob = side;
				in.mode = 3;
				in.blocks = 4;
				in.reach = steps[s].reach;
				in.plausible = 1;
				w = probe(probe_run, &in);
				printf("    %-42s %-4s: ", steps[s].what,
				       side ? "BLOB" : "ours");
				if (w.signo || !w.done) {
					char buf[96];

					verdict_str(&w, buf, sizeof buf);
					printf("%s\n", buf);
					nfail++;
					continue;
				}
				printf("state=%ld eventCode=%ld "
				       "symbolCount=%ld\n",
				       shared->v[26], shared->v[27],
				       shared->v[28]);
				printf("        %ld non-zero samples of %d, "
				       "%ld NaN, range [%g, %g]\n",
				       shared->v[24],
				       4 * (int)shared->v[2], shared->v[25],
				       shared->f[0], shared->f[1]);
				ran++;
			}
		}
		/*
		 * COUNTED, not asserted true.  A `must(1, ...)` here would
		 * print "yes" whether or not any arm ran, which is 7626's own
		 * subject and would be a poor advertisement for it.
		 */
		must(ran == 4, "all four (step x side) arms ran to the end");
	}

	/* --------------------------------------------------- milestone 3 */
	printf("\n[3] WHERE DOES THE MAPPING-PARAMS BLOCKER BITE?\n");
	printf("    Nothing on the digital side writes V90MappingParams "
	       "(7520, 7570, 7581).\n");
	printf("    Three controlled fills of the block, then the same three "
	       "with the\n    harness supplying setParamsInfoFromCPUnPck's "
	       "missing call.\n");
	{
		static const struct {
			int fill;
			const char *what;
		} fills[] = {
		    { 0x00, "0x00 (a fresh page)" },
		    { 0xa5, "0xa5 (the harness allocator's own fill)" },
		    { 0xff, "0xff" },
		};
		unsigned f;
		int unpack, side;

		for (unpack = 0; unpack < 2; unpack++) {
			printf("\n    --- %s ---\n", unpack
			       ? "AFTER setParamsInfoFromCPUnPck, from the "
				 "harness"
			       : "as the constructor left it");
			for (f = 0; f < sizeof fills / sizeof fills[0]; f++) {
				for (side = 0; side < 2; side++) {
					char buf[96];

					memset(&in, 0, sizeof in);
					in.digitalSide = 1;
					in.useBlob = side;
					in.mode = 3;
					in.blocks = 32;
					in.reach = REACH_DATA;
					in.fill = fills[f].fill;
					in.unpack = unpack;
					w = probe(probe_run, &in);
					verdict_str(&w, buf, sizeof buf);
					printf("      fill %-42s %-4s: %s\n",
					       fills[f].what,
					       side ? "BLOB" : "ours", buf);
					if (w.done)
						report_run(&in);
				}
			}
		}
		/* And the control: a block that IS filled, by hand. */
		printf("\n    --- the control: a plausible block poked in by "
		       "hand ---\n");
		for (side = 0; side < 2; side++) {
			char buf[96];

			memset(&in, 0, sizeof in);
			in.digitalSide = 1;
			in.useBlob = side;
			in.mode = 3;
			in.blocks = 32;
			in.reach = REACH_DATA;
			in.fill = -1;
			in.plausible = 1;
			w = probe(probe_run, &in);
			verdict_str(&w, buf, sizeof buf);
			printf("      plausible_mapping(42 bits/frame)  "
			       "%-4s: %s\n", side ? "BLOB" : "ours", buf);
			if (w.done)
				report_run(&in);
			must(w.done == 1,
			     "the data phase runs when the block is filled");
		}

		/*
		 * AND THE SAME TWO ARMS ENTERED AT THE CHAIN'S REAL HEAD.
		 * Everything above calls `V90BitsToSymbol::reset` directly,
		 * which is the SECOND link of the chain 7520 names.  These
		 * two go through `V90Phase4Modulator::setMappingParams`, the
		 * first, so the claim does not have to carry a caveat about
		 * the link that was skipped.
		 */
		printf("\n    --- the chain entered at its real head, "
		       "V90Phase4Modulator::setMappingParams ---\n");
		for (side = 0; side < 2; side++) {
			char buf[96];
			int u;

			for (u = 0; u < 2; u++) {
				memset(&in, 0, sizeof in);
				in.digitalSide = 1;
				in.useBlob = side;
				in.mode = 3;
				in.blocks = 32;
				in.reach = REACH_DATA;
				in.fill = 0xa5;
				in.unpack = u;
				in.viaSetMapping = 1;
				w = probe(probe_run, &in);
				verdict_str(&w, buf, sizeof buf);
				printf("      fill 0xa5, %-24s %-4s: %s\n",
				       u ? "unpacker called"
					 : "as the ctor left it",
				       side ? "BLOB" : "ours", buf);
				if (w.done)
					report_run(&in);
			}
		}
	}

	/* ------------------------------------------- the driver question */
	printf("\n[3b] CAN EITHER DRIVER BE ENTERED ON A DIGITAL "
	       "INSTANCE?\n");
	printf("    Both dispatch on modem.demodulator->word_3c, and the "
	       "digital side\n    has no demodulator.  7581 argued from the "
	       "jump tables; this asks\n    the blunter question one level "
	       "up.\n");
	{
		int which, side, digital, force;

		for (digital = 1; digital >= 0; digital--)
		    for (which = 1; which >= 0; which--)
			for (force = 0; force < 2; force++)
			    for (side = 0; side < 2; side++) {
				char buf[96];

				if (which == 1 && force == 1)
					continue;	/* no early return */
				memset(&in, 0, sizeof in);
				in.digitalSide = digital;
				in.useBlob = side;
				in.mode = 3;
				in.which = which;
				in.forceDispatch = force;
				w = probe(probe_driver, &in);
				verdict_str(&w, buf, sizeof buf);
				printf("      %-7s %-17s %-13s %-4s: %s",
				       digital ? "DIGITAL" : "analog",
				       which ? "v90RunDemodulator"
					     : "runPcmModem",
				       which ? ""
					     : (force ? "info0Layout=1"
						      : "as constructed"),
				       side ? "BLOB" : "ours", buf);
				if (w.done)
					printf(" (returned %ld, "
					       "demodulator=%ld)",
					       shared->v[2], shared->v[0]);
				printf("\n");
			    }
	}

	/* --------------------------------------------------- milestone 4 */
	printf("\n[4] LOOPBACK: a digital instance against an analogue one, "
	       "both OURS\n");
	printf("    LIMITATION, stated before the numbers: both sides are our "
	       "source, so\n    a defect they share is invisible -- a "
	       "reversed bit order or a CRC over\n    the wrong extent would "
	       "train happily.  This proves function, never\n    "
	       "conformance.  The rates do not match either: the digital arm "
	       "emits 80\n    codewords a block at 8 kHz and the analogue arm "
	       "wants 96 at 9600.\n");
	{
		char buf[96];
		long moved[2] = { -1, -1 };
		long seq[2][8];
		int silent, seqdiff = 0;

		memset(seq, 0, sizeof seq);

		for (silent = 0; silent < 2; silent++) {
			memset(&in, 0, sizeof in);
			in.blocks = 8;
			in.faultOnPurpose = silent;	/* the control */
			w = probe(probe_loopback, &in);
			verdict_str(&w, buf, sizeof buf);
			printf("    --- %s ---\n      %s\n",
			       silent ? "THE NEGATIVE CONTROL: the same eight "
					"blocks, link zeroed"
				      : "the digital modulator's own output",
			       buf);
			if (!w.done)
				continue;
			moved[silent] = shared->v[25];
			printf("      digital: modulator=%ld demodulator=%ld "
			       "block=%ld\n",
			       shared->v[2], shared->v[3], shared->v[6]);
			printf("      analogue: modulator=%ld "
			       "demodulator=%ld word_3c before=0x%lx\n",
			       shared->v[4], shared->v[5],
			       (unsigned long)shared->v[7]);
			printf("      %ld non-zero codewords emitted, "
			       "demodulator event moved %ld times of %d\n",
			       shared->v[26], shared->v[25], in.blocks);
			printf("      word_3c after each block:");
			for (i = 0; i < in.blocks && i < 8; i++) {
				seq[silent][i] = shared->v[16 + i];
				printf(" %ld", shared->v[16 + i]);
			}
			printf("\n");
			printf("      final: modulator state=%ld "
			       "eventCode=%ld; demodulator word_3c=%ld "
			       "inPhase3=0x%lx\n",
			       shared->v[27], shared->v[28], shared->v[29],
			       (unsigned long)shared->v[30]);
			printf("      nothing left allocated: %ld\n",
			       shared->v[31]);
		}
		seqdiff = memcmp(seq[0], seq[1], sizeof seq[0]) != 0;
		printf("\n      VERDICT, and the COUNT and the SEQUENCE do "
		       "not agree:\n");
		printf("        movements: signal %ld, silence %ld -- %s, so "
		       "the COUNT\n        alone is not evidence of a "
		       "reaction to the signal.\n", moved[0], moved[1],
		       moved[0] == moved[1] ? "IDENTICAL" : "different");
		printf("        the SEQUENCE of event words is %s, so the "
		       "input %s\n        reach the demodulator's state.\n",
		       seqdiff ? "DIFFERENT" : "identical",
		       seqdiff ? "does" : "does NOT");
		must(moved[0] >= 0 && moved[1] >= 0,
		     "BOTH loopback arms ran, so the control is a comparison");
	}

	/* ------------------------------- the event-code vocabulary (7581) */
	printf("\n[4b] WHICH EVENT CODES DOES THE DIGITAL MODULATOR "
	       "RAISE?\n");
	printf("    v90RunDemodulator's table is 0x00..0x2b and "
	       "runPcmModem's is\n    0x00..0x35, both on the same "
	       "demodulator field (7581).\n");
	{
		char buf[96];

		memset(&in, 0, sizeof in);
		in.digitalSide = 1;
		in.mode = 3;
		in.blocks = 400;
		w = probe(probe_eventcodes, &in);
		verdict_str(&w, buf, sizeof buf);
		printf("      %s\n", buf);
		if (w.done) {
			printf("      over %ld blocks, codes seen:",
			       shared->v[40]);
			for (i = 0; i < 40; i++)
				if (shared->v[i] != 0)
					printf(" %d(x%ld)", i, shared->v[i]);
			printf("\n      highest code raised: %ld; final "
			       "modulator state %ld\n",
			       shared->v[42], shared->v[41]);
		}
	}

	printf("\n=== %d harness assertion(s) failed ===\n", nfail);
	printf("    Read the tables above, not this line: a probe that DIES "
	       "is a\n    result here and is not counted as a failure.  This "
	       "counts only\n    the apparatus's own checks and milestone "
	       "1's field readings.\n\n");
	return nfail != 0;
}
