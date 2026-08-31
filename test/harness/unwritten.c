/*
 * unwritten.c -- bridges from unwritten callees' REAL names to the blob's
 * ref_ aliases, so a reconstructed caller can link and run before its
 * callees are reconstructed.
 *
 * WHY THIS EXISTS.  symmap.py renames every symbol the blob defines to
 * ref_*, so an unwritten callee's real name resolves to nothing in a test
 * binary: writing `CID_process`, which calls `cid_progress`
 * unconditionally, would otherwise fail 60+ links at once -- finding F217's
 * wall, which for `v34handshak`'s closure was answered by writing the
 * callees first (F215).  For the CID wrapper and the `prop_dp_*` aggregate
 * the callees are the whole FSK caller-ID receiver and the whole V.22 and
 * V.32 datapumps, far outside those functions' own batch, so this file is
 * F214's documented escape hatch at the harness tier instead: each bridge
 * forwards the real name to the blob's copy.
 *
 * WHAT IT MEANS FOR A TEST.  A bridged callee runs BLOB code whichever side
 * called it, so its stateful host callbacks (modem_dp_register,
 * dsplibs_debug_printf...) go through the ref_ imports and land on the
 * REFERENCE side's logs and transcripts even for our-side calls.  t_dpinit
 * and t_cid are written against exactly that split and say so.
 *
 * WHAT IT MEANS FOR src/.  Nothing.  The reconstruction declares these
 * callees weak (the DSPLIB_*_UNWRITTEN idiom) and calls them by their real
 * names; in the interop binaries, which link no blob and never reach these
 * paths, the weak references resolve to zero.  No ref_ name appears
 * anywhere in src/.
 *
 * WHEN A BRIDGED SYMBOL IS RECONSTRUCTED its definition collides with the
 * bridge and every link fails LOUDLY -- delete the bridge here, and the
 * test split it served collapses back to straight symmetry (t_dpinit's
 * header comment walks through it).
 */

/* --- the Caller ID receiver, span cid_modem.c (0x8fd60..) ------------- */

extern void *ref_cid_create(void *m, unsigned cid_val, int w);
extern void ref_cid_delete(void *cid);
extern void ref_cid_freq_sampl(void *cid, int rate);
extern short ref_cid_progress(void *cid, short *in, int what, short *count);
extern char *ref_cid_get_strings(void *cid);

void *
cid_create(void *m, unsigned cid_val, int w)
{
	return ref_cid_create(m, cid_val, w);
}

void
cid_delete(void *cid)
{
	ref_cid_delete(cid);
}

void
cid_freq_sampl(void *cid, int rate)
{
	ref_cid_freq_sampl(cid, rate);
}

short
cid_progress(void *cid, short *in, int what, short *count)
{
	return ref_cid_progress(cid, in, what, count);
}

char *
cid_get_strings(void *cid)
{
	return ref_cid_get_strings(cid);
}

/* --- the V.22 and V.32 registration pairs (0x5360/0x53b0, 0x4bb0/0x4bf0),
 *     for prop_dp_init/prop_dp_exit -------------------------------------- */

extern int ref_dp_v22_init(void);
extern void ref_dp_v22_exit(void);

int
dp_v22_init(void)
{
	return ref_dp_v22_init();
}

void
dp_v22_exit(void)
{
	ref_dp_v22_exit();
}

