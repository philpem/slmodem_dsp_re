/*
 * v34hsstep.h -- drive ONE dispatch case of `v34handshak` and step it once.
 *
 * `v34handshak` is 61,541 bytes, the largest function in the object, and it
 * is a dispatch over three concurrent state machines rather than one body.
 * Reconstructing it as a unit has never been attempted here because the
 * measurement in finding 220 says it cannot be: context growth is cumulative
 * output converging to 1:1, so the wall is a TURN count, and a function this
 * size is hundreds of turns however carefully it is read.
 *
 * The way out is per-dispatch-case testing, and this is the fixture for it.
 * The three state words are plain halfwords in an object the test allocates,
 * so a test can write one, call the function once, and compare -- which turns
 * table 3's 27.6 KB into sixteen independently committable units instead of
 * one that has to work entirely before anything passes.
 *
 * WHAT IT DOES NOT DO.  It does not reconstruct anything.  `v34handshak` has
 * no reconstruction yet, so BOTH sides call the blob today; see `v34hs_step`
 * for the one-line swap that makes side A the reconstruction, and
 * docs/v34handshak.md for what a per-case agent does with it.
 *
 * Blob-against-blob is not a vacuous comparison here, because the two sides
 * are at DIFFERENT ADDRESSES and are brought up by DIFFERENT CODE: side A is
 * initialised by this tree's `v34handshakinit` and side B by the blob's.  An
 * agreeing step therefore says the fixture is deterministic, address
 * independent and fully seeded -- the three things a per-case agent has to be
 * able to assume before its own failures mean anything.
 */

#ifndef DSPLIB_TEST_V34HSSTEP_H
#define DSPLIB_TEST_V34HSSTEP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ---------------------------------------------------------------------------
 * The three state halfwords (finding 213), and the guards that decide which
 * of the three dispatches sees them.
 *
 * Read off the prologue at 0x628f0 rather than assumed:
 *
 *   62925  movzwl 0x2aa0(%ebx),%edx      ; the transmit sample limit
 *   62933  cmp    %dx,0x221c(%ebx)       ; against the transmit cursor
 *   6293e  jge    629ed                  ; cursor >= limit: once-per-block
 *   62957  movswl 0x3596(%esi),%eax      ; else txstate, table 1
 *
 *   629f1  cmpw   $0x5,(%ebx)            ; ebx = obj+0x264
 *   629f5  jle    62ae3                  ; <= 5: txstate, table 2
 *   62a02  movswl 0x3594(%edx),%eax      ; else the rxstate compare chain
 */
#define V34HS_MICROSTATE	0x3592
#define V34HS_RXSTATE		0x3594
#define V34HS_TXSTATE		0x3596

#define V34HS_TXCURSOR		0x221c	/* short: samples emitted so far   */
#define V34HS_TXLIMIT		0x2aa0	/* short: how many the block wants */
#define V34HS_RXCOUNT		0x0264	/* short: receiver +0x00           */
#define V34HS_FSKGATE		0xa8a0	/* int:  non-zero diverts at 64a87 */
#define V34HS_PROGRESS		0x0004	/* int:  the code the step reports */

/*
 * The one rxstate that reaches the microstate table.  0x64a64 runs V34agc and
 * `fskdemodulate` and only then reads +0x3592, so this is the route to all
 * sixteen of table 3's cases.  Neither of those two functions writes any of
 * the three state words -- swept over the whole of .text, finding 285 -- so a
 * microstate written before the call is still there when the dispatch reads
 * it.
 */
#define V34HS_RX_DPSK		43	/* -> 0x64a64 -> table 3 */
#define V34HS_RX_RECEIVE	4	/* -> 0x653e4            */
#define V34HS_RX_WAIT		35	/* -> 0x6752c            */

enum v34hs_route {
	/*
	 * Table 1, .rodata+0x2da0: txstate INSIDE the per-sample loop.  This
	 * is #56, it compares, and eighteen of its nineteen reachable targets
	 * have their own behaviour cold (finding 323).
	 *
	 * DANGEROUS AND DELIBERATELY SO -- see finding 287.  The loop bottom
	 * at 0x629e0 re-tests the cursor against the limit and jumps back to
	 * the dispatch, and the default arm IS that bottom, so a txstate with
	 * no case of its own spins forever.  `v34hs_route` takes the sample
	 * budget explicitly for this reason and `v34hs_step` arms an alarm.
	 */
	V34HS_ROUTE_TXSAMPLE,
	/* Table 2, .rodata+0x2ee8: txstate once per block. */
	V34HS_ROUTE_TXBLOCK,
	/*
	 * The rxstate compare chain, which has no table.  With rxstate
	 * V34HS_RX_DPSK this is the route to table 3, .rodata+0x3000, the
	 * microstate dispatch -- 27.6 KB and sixteen cases, which is #57.
	 */
	V34HS_ROUTE_RXCHAIN
};

/* What one step did, per side. */
struct v34hs_obs {
	unsigned	changed;	/* object bytes the step wrote       */
	unsigned	first;		/* lowest offset written, ~0u if none*/
	unsigned	last;		/* highest offset written            */
	unsigned	hash;		/* FNV-1a over (offset, new byte)    */
	unsigned	lines;		/* diagnostic lines printed          */
	short		mst, rxst, txst;/* the three words afterwards        */
	int		progress;	/* the int at obj+0x04 afterwards    */
};

/*
 * Build both objects and bring them up.  `mode` is `v34handshakinit`'s: 0 is
 * the cold start every case here uses, 1 retrain, 2 rate renegotiation, 4
 * Modem-on-Hold.
 *
 * The objects are filled with VARIED pseudorandom bytes from a fixed LCG and
 * never zeroed, both sides identically (finding 230): zero is the one value
 * that makes a field nothing has written look deliberate.
 */
void v34hs_setup(int mode);

/* Set the guards so the named dispatch is the one that runs. */
void v34hs_route(enum v34hs_route r, short samples);

/*
 * Put OUR reconstruction on side A, or NULL to put the blob back (the
 * default).  This is what makes a per-case test an ordinary tier-1
 * differential test while the rest of `v34handshak` does not exist: the
 * compile-time `V34HS_OURS` swap replaces side A in EVERY binary that links
 * this fixture, so it cannot be turned on until the whole 61,541 bytes are
 * written, and four batches are landing arms in parallel under four names.
 *
 * A test that calls this should also run the same case with NULL, so that a
 * pass says our arm agrees with the blob rather than that the fixture agrees
 * with itself.
 */
void v34hs_side_a(void (*fn)(void *obj));

/*
 * REPLACE THE ENTRY POINT ON BOTH SIDES, for a function that is not
 * `v34handshak` but shares its object.
 *
 * `v34hs_side_a` and `v34hs_step_case` both assume the thing being stepped is
 * `v34handshak`, and differ only in what side A runs instead of it.
 * `datapumpv34` is the function that CALLS `v34handshak`, in the same
 * translation unit and against the same object, and what it needs from this
 * fixture is the arena: findings 319-322 say an object step depends on the
 * geometry of the five blocks the object points at, so a second fixture would
 * mean building that geometry a second time and being wrong about it once.
 *
 * `a` runs on side A and `b` on side B -- ours and the blob's.  `log_a` is
 * the capture channel side A writes: 0 for our code, 1 for the blob's, which
 * a test running the blob on BOTH sides as its control needs, because
 * comparing an empty slot 0 against a full slot 1 fails the control for a
 * reason that is not about the object.  Not composable with the two per-case
 * forms; a test uses this alone.  `v34hs_entry(NULL, NULL, 0)` puts
 * `v34handshak` back.
 */
void v34hs_entry(void (*a)(void *obj), void (*b)(void *obj), int log_a);

/* Write the three state halfwords on both sides. */
void v34hs_state(short mst, short rxst, short txst);

/* Call the function once on each side and record what it did. */
void v34hs_step(void);

/*
 * PUT A RECONSTRUCTION ON SIDE A, for one dispatch case at a time.
 *
 * `V34HS_OURS` is the whole-function swap and it cannot be what a per-case
 * agent uses: `v34handshak` would have to exist and be right for EVERY case
 * before one case could be tested through it, which is the sitting the
 * per-case split exists to avoid.  This is the per-case form.
 * `t_v34hsstep.c` leaves it NULL and goes on proving the fixture; a test that
 * has reconstructed one dispatch installs its own entry point and drives only
 * the states that dispatch owns.
 *
 * It also moves side A's transcript to the capture slot OUR code writes.
 * With the blob on both sides the two slots are 1 and 1; with a
 * reconstruction on side A they are 0 and 1, and leaving that at 1 would
 * compare the blob's transcript against itself, which passes by construction.
 * NULL restores the blob and the slot together.
 */

/*
 * The same step with one RECONSTRUCTED DISPATCH ARM on side A, run before the
 * blob and inside the same snapshot, alarm and observation.  Returns whatever
 * the arm returned.
 *
 * This is how a single case of `v34handshak` is compared while the function
 * as a whole does not exist: an arm that leaves the transmit queue's count at
 * the block's limit makes the blob's own guard at 0x62933 skip the per-sample
 * loop, so side A gets our arm and the blob's tail where side B gets the
 * blob's arm and the same tail.  See the comment on the definition, and
 * test/unit/t_v34hstx1.c for the two guards a test using it needs against
 * passing vacuously.
 *
 * The diagnostics must be OFF: our code and the blob's log to two different
 * capture channels and only one of them reaches the transcript comparison.
 */
int v34hs_step_case(int (*arm)(void *));

/*
 * Compare the two sides: the whole object byte for byte with the pointer
 * fields excluded, the two self-pointers by offset-from-own-base, and both
 * transcripts.  This is the check that says the fixture is sound.
 */
void v34hs_compare(const char *what, long tag);

/*
 * How many pointer fields the comparison skips, and the assertion that every
 * one of them was reached.  Call it ONCE, after the whole sweep.
 *
 * WHICH TABLE A POINTER SELECTS IS CHECKED (finding 324).  Each of the
 * thirty-seven is classified three ways: into its own object, into its own
 * arena -- where the offset says which block and where in it, and offsets are
 * comparable where addresses are not -- or outside both, which is a library
 * table or function.  For that last class side A holds ours and side B the
 * blob's, so what checks it is a SECOND PASS with `v34hs_refinit(1)`: brought
 * up by the same code the two must select the identical address, and all
 * twelve that qualify do.  `t_v34hsstep.c` runs that pass, so the check is in
 * `make phase` rather than behind an environment variable.
 *
 * `v34hs_holes_check` does not apply to a refinit pass: eleven of the skips
 * legitimately never differ once both sides install the same table.
 */
#define V34HS_NHOLES	37
void v34hs_holes_check(void);

const struct v34hs_obs *v34hs_observed(int side);
const char *v34hs_text(int side);

/* Raw access, for a case that needs a companion field set up. */
void *v34hs_object(int side);
void v34hs_poke_short(unsigned off, short v);
void v34hs_poke_int(unsigned off, int v);
void v34hs_poke_byte(unsigned off, unsigned char v);
/*
 * Aim a pointer field inside the object at that side's own object plus
 * `target`.  A raw address written into both sides would make the two
 * geometries differ, which is the one thing this fixture exists to avoid.
 */
void v34hs_poke_self_ptr(unsigned off, unsigned target);
short v34hs_peek_short(int side, unsigned off);

/*
 * Bring side A up with the blob's initialisers too, so both sides install the
 * same library tables.  That is the run in which "which table does this
 * pointer select" is a comparison rather than a shrug; `v34hs_holes_check`
 * does not apply to it.  Finding 324.
 */
void v34hs_refinit(int on);

/*
 * AND THE MIRROR OF IT: bring side B up with OUR initialisers.
 *
 * `v34hs_refinit` moves side A to the blob; this moves side B to ours, and
 * the two together give all four combinations of who brings up which side.
 * That is what a test needs when the two sides are not ours-and-the-blob but
 * the TWO ENDPOINTS OF ONE CALL -- `t_v34call.c` runs the same call four
 * ways (ours on both, the blob on both, and each mixed pair) and compares the
 * runs against each other rather than the sides against each other.  Off by
 * default, so every existing user is unaffected.
 */
void v34hs_oursinit(int on);

/*
 * Is this object offset inside one of the thirty-seven pointer skips?
 *
 * `v34hs_compare` excludes them because two objects at two addresses hold two
 * different addresses in each, necessarily and forever.  A test comparing two
 * sequential RUNS over the same memory needs the same exclusion for a
 * different reason: our bring-up installs OUR library tables and the blob's
 * installs the blob's, so the pointer fields hold two addresses of two copies
 * across the runs.  Exposed rather than duplicated -- a second copy of the
 * list is a second copy to go stale.
 */
int v34hs_in_hole(unsigned off);

/*
 * FNV-1a over ONE SIDE's whole arena: the object with the thirty-seven
 * pointer skips excluded, the five blocks it points at, and the seven filler
 * regions between and around them.
 *
 * This is `v34hs_compare`'s coverage expressed as a number rather than as a
 * side-against-side check, for a test whose two sides are the two endpoints
 * of a call and so must NOT be compared against each other.  What it is
 * compared against is the same side in another RUN over the same memory,
 * where every address is identical and the four bytes of the session's
 * pointer to the PCM block are the only thing that has to be exempt.
 *
 * The object's hash is folded in first, so a difference inside the object and
 * one outside it are not interchangeable.
 */
unsigned v34hs_arena_hash(int side);

/*
 * The seven filler regions on their own.  They are 224 KB of the arena's 316
 * and no step writes any of them (finding 322), so they are a claim worth
 * making ONCE per run rather than once per step, and keeping them out of
 * `v34hs_arena_hash` is what makes that hash cheap enough to take every
 * block.
 */
unsigned v34hs_padding_hash(int side);

/*
 * Turn the diagnostics on for both sides.  `v34handshak` indexes `StateName`
 * unbounded (D42), so every state word must stay in 0..86 while this is on.
 */
/*
 * Put THIS TREE'S `v34handshak` on side A instead of the blob's, for this
 * binary only.  Off by default, which is the fixture proving itself.
 *
 * `v34handshak` is partial and halts on an arm nobody has written, so a test
 * that turns this on must drive only states some batch has landed;
 * docs/v34handshak.md says which.
 */
void v34hs_ours(int on);

void v34hs_debug(int on);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_TEST_V34HSSTEP_H */
