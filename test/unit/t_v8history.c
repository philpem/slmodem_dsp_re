/* Real native-rate V.8 component histories. Two independent calls run in
 * lockstep: original caller/answerer and reconstructed caller/answerer.
 * Only the peer's previous output reaches each receiver; no protocol fields
 * or V8Process results are planted. Host callbacks only observe and forward.
 * Continuing V.8 through its idle window is a wrapper component contract,
 * not evidence about a public modem's datapump replacement scheduler. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "harness.h"
#include "dsplib/v8dp.h"

extern void ref_dp_v8_init(void);
extern void ref_dp_v8_exit(void);
extern struct v8 *ref_V8Create(const struct v8_cfg *);
extern void ref_V8Delete(struct v8 *);
extern int ref_V8Process(struct v8 *, const short *, short *, int);
extern int ref_V8UpdateModemParameters(struct v8 *, struct v8_cm *);

#define FRAME 192
#define FRAMES 700
#define EVENTS 8
struct observation {
	int write, parameter, value, modem_matches;
	int idle_timer, f1c, last_status;
	struct dsp_info info;
	struct v8_cm menu;
};
static struct v8_dp *active[2];
static struct observation observations[2][EVENTS];
static int events[2], overflow, negative;
static int modem_handle[2];
static long observed;
static struct v8 handshake_copy[2];

static void observe(int side, int write, void *modem, unsigned parameter, int value)
{
	struct observation *o;
	struct v8_dp *v = active[side];
	if (!v) return;
	if (events[side] >= EVENTS) { overflow = 1; return; }
	o = &observations[side][events[side]++];
	memset(o, 0, sizeof *o);
	o->write = write; o->parameter = parameter; o->value = value;
	o->modem_matches = modem == &modem_handle[v->answerer];
	o->idle_timer = v->idle_timer; o->f1c = v->f1c;
	o->last_status = v->last_status; o->info = *v->dspinfo; o->menu = *v->cm;
	/* Fixed observer canary, activated only by an explicit negative run. */
	if (negative && !side && write && (value == DP_V34 || value == DP_V32)) o->info.qc_index ^= 1;
}
extern long __real_modem_get_param(void *, unsigned);
extern long __real_ref_modem_get_param(void *, unsigned);
extern long __real_modem_set_param(void *, unsigned, int);
extern long __real_ref_modem_set_param(void *, unsigned, int);
long __wrap_modem_get_param(void *m, unsigned p)
{ observe(0, 0, m, p, 0); return __real_modem_get_param(m, p); }
long __wrap_ref_modem_get_param(void *m, unsigned p)
{ observe(1, 0, m, p, 0); return __real_ref_modem_get_param(m, p); }
long __wrap_modem_set_param(void *m, unsigned p, int v)
{ observe(0, 1, m, p, v); return __real_modem_set_param(m, p, v); }
long __wrap_ref_modem_set_param(void *m, unsigned p, int v)
{ observe(1, 1, m, p, v); return __real_ref_modem_set_param(m, p, v); }

static void normalise(struct v8_dp *copy, const struct v8_dp *v)
{
	*copy = *v;
	copy->modem = 0; copy->op = 0; copy->self = 0;
	copy->dspinfo = 0; copy->cm = 0; copy->v8 = 0;
}

/* Preserve internal pointer offsets; immutable tables live in distinct images.
 * This follows t_v8direct: uninitialised allocator-fill values stay untouched. */
static void *relative(const void *pointer, const struct v8 *v)
{
	uintptr_t p = (uintptr_t)pointer, base = (uintptr_t)v;
	return p >= base && p < base + sizeof *v ? (void *)(p - base + 1) : (void *)p;
}
static void normalise_handshake(struct v8 *copy, const struct v8 *v)
{
	*copy = *v;
	copy->tx_sym_a = relative(v->tx_sym_a, v);
	copy->tx_sym_b = relative(v->tx_sym_b, v);
	copy->tx_ring_base = relative(v->tx_ring_base, v);
	copy->tx_ring_half = relative(v->tx_ring_half, v);
	copy->rx.buf = relative(v->rx.buf, v);
	copy->tx_seq = relative(v->tx_seq, v);
	copy->seq_alt = relative(v->seq_alt, v);
	copy->seq_spare = relative(v->seq_spare, v);
	copy->detector.table = (const short *)(uintptr_t)(v->detector.table != 0);
	copy->v21.a = (const short *)(uintptr_t)(v->v21.a != 0);
	copy->v21.b = (const short *)(uintptr_t)(v->v21.b != 0);
	copy->v21.c = (const short *)(uintptr_t)(v->v21.c != 0);
	copy->v21.d = (const short *)(uintptr_t)(v->v21.d != 0);
	copy->cm = 0;
}

/* The wrapper always advertises both V.34 and V.32. A V.32-only offer
 * therefore comes from a normally configured public-interface peer, never
 * from editing a wrapper's handshake/menu after construction. -1 uses two
 * wrappers; 0/1 chooses the wrapper role and the other role is the peer. */
static int run_history(int wrapper_endpoint)
{
	struct dp *dp[2][2] = {{0}};
	struct dp_operations *ops[2] = {0};
	struct v8 *handshake[2][2] = {{0}};
	int peer_updated[2][2] = {{0}};
	int expected_mode = wrapper_endpoint == -1 ? DP_V34 : DP_V32;
	struct v8_cm cm[2][2];
	struct dsp_info info[2][2];
	static short air[2][2][FRAME], out[2][2][FRAME];
	int side, endpoint, frame, rc = 0;
	int linked[2] = {0}, expired[2] = {0}, first[2] = {-1,-1};
	long statuses[2][V8_LAST_ENUM + 1] = {{0}};
	observed = 0;
	overflow = 0;
	memset(air, 0, sizeof air);
	memset(out, 0, sizeof out);
	memset(cm, 0, sizeof cm); memset(info, 0, sizeof info);
	diff_begin(wrapper_endpoint == -1 ? "V8: real V34 wrapper pair"
		: wrapper_endpoint == 0 ? "V8: V32-only answerer/public peer"
		: "V8: V32-only caller/public peer");
	harness_reg_reset();
	dp_v8_init();
	ref_dp_v8_init();
	for (side = 0; side < 2; side++) {
		struct reg_log *log = side ? &harness_reg_ref : &harness_reg_ours;
		int i;
		for (i = 0; i < log->count; i++)
			if (log->id[i] == DP_V8)
				ops[side] = log->ops[i];
		diff_eq_int("V8 operations registered (%ld)", ops[side] != 0, 1, side);
		if (!ops[side]) goto done;
	}
	for (side = 0; side < 2; side++) for (endpoint = 0; endpoint < 2; endpoint++) {
		cm[side][endpoint].b0 = 0xa0; cm[side][endpoint].b1 = 0x70;
		cm[side][endpoint].b2 = 0x40;
		/* The host-owned result block contains a previous result. Negotiation
		 * must publish the new zero values before requesting the transition. */
		info[side][endpoint].qc_lapm = 1;
		info[side][endpoint].qc_index = 17;
		if (wrapper_endpoint == -1 || endpoint == wrapper_endpoint) {
			harness_param_reset(); harness_param_set(5, 400);
			harness_param_set(MDMPRM_DPRUNTIME, (long)(intptr_t)&cm[side][endpoint]);
			harness_param_set(MDMPRM_DSPINFO, (long)(intptr_t)&info[side][endpoint]);
			dp[side][endpoint] = ops[side]->create(&modem_handle[endpoint], DP_V34,
				!endpoint, V8_DP_RATE, FRAME, ops[side]);
			if (dp[side][endpoint])
				handshake[side][endpoint] = ((struct v8_dp *)dp[side][endpoint])->v8;
		} else {
			struct v8_cfg cfg;
			memset(&cfg, 0, sizeof cfg);
			cm[side][endpoint].b0 = 0x80; /* Offer V.32, omit V.34. */
			cfg.side = endpoint;
			cfg.timeout_a = 12;
			cfg.timeout_b = 7;
			cfg.rate = V8_DP_RATE;
			cfg.cm = &cm[side][endpoint];
			handshake[side][endpoint] = side ? ref_V8Create(&cfg) : V8Create(&cfg);
		}
		diff_eq_int("constructor built endpoint (%ld)", handshake[side][endpoint] != 0, 1, side*2+endpoint);
		if (!handshake[side][endpoint]) goto done;
	}
	for (frame = 0; frame < FRAMES; frame++) {
		for (endpoint = 0; endpoint < 2; endpoint++) {
			int result[2], i;
			struct v8_dp a, b;
			memset(observations, 0, sizeof observations); events[0] = events[1] = 0;
			for (side = 0; side < 2; side++) {
				if (dp[side][endpoint]) {
					active[side] = (struct v8_dp *)dp[side][endpoint];
					result[side] = ops[side]->process(dp[side][endpoint],
						air[side][endpoint], out[side][endpoint], FRAME);
					active[side] = 0;
				} else {
					struct v8 *v = handshake[side][endpoint];
					result[side] = side ? ref_V8Process(v, air[side][endpoint], out[side][endpoint], FRAME)
						: V8Process(v, air[side][endpoint], out[side][endpoint], FRAME);
					if (result[side] == V8_OK && !peer_updated[side][endpoint]) {
						int updated = side ? ref_V8UpdateModemParameters(v, &cm[side][endpoint])
							: V8UpdateModemParameters(v, &cm[side][endpoint]);
						diff_eq_int("public peer consumed negotiated menu (%ld)", updated, 0, side);
						peer_updated[side][endpoint]++;
					}
				}
			}
			diff_eq_int("process result (%ld)", result[0], result[1], frame*2+endpoint);
			diff_eq_obj("output samples", short[FRAME], out[0][endpoint], out[1][endpoint], frame*2+endpoint);
			if (dp[0][endpoint] && dp[1][endpoint]) {
				normalise(&a, (struct v8_dp *)dp[0][endpoint]);
				normalise(&b, (struct v8_dp *)dp[1][endpoint]);
				diff_eq_obj("wrapper state", struct v8_dp, &a, &b, frame*2+endpoint);
			}
			for (side = 0; side < 2; side++) {
				struct v8 *h = handshake[side][endpoint];
				diff_eq_int("handshake menu ownership (%ld)", h->cm == &cm[side][endpoint], 1, frame*4+endpoint*2+side);
				if (dp[side][endpoint]) {
					struct v8_dp *v = (struct v8_dp *)dp[side][endpoint];
					diff_eq_int("wrapper self ownership (%ld)", v->self == v, 1, frame*4+endpoint*2+side);
					diff_eq_int("registered operation ownership (%ld)", v->op == ops[side], 1, frame*4+endpoint*2+side);
					diff_eq_int("host publication ownership (%ld)", v->dspinfo == &info[side][endpoint], 1, frame*4+endpoint*2+side);
				}
				normalise_handshake(&handshake_copy[side], h);
			}
			diff_eq_obj("complete handshake", struct v8, &handshake_copy[0], &handshake_copy[1], frame*2+endpoint);
			diff_eq_obj("menu", struct v8_cm, &cm[0][endpoint], &cm[1][endpoint], frame*2+endpoint);
			diff_eq_obj("publication", struct dsp_info, &info[0][endpoint], &info[1][endpoint], frame*2+endpoint);
			diff_eq_int("callback count (%ld)", events[0], events[1], frame*2+endpoint);
			for (i = 0; i < events[0] && i < events[1]; i++) {
				struct observation *o = &observations[1][i];
				diff_eq_obj("read-only host callback", struct observation, &observations[0][i], o, frame*2+endpoint);
				diff_eq_int("callback modem (%ld)", o->modem_matches, 1, frame*2+endpoint);
				if (o->write && o->parameter == 9 && o->value == expected_mode) {
					linked[endpoint]++; first[endpoint] = frame;
					diff_eq_int("link callback is first (%ld)", i, 0, endpoint);
					diff_eq_int("negotiated modulation (%ld)", o->menu.b0 & 0xa0,
						expected_mode == DP_V34 ? 0xa0 : 0x80, endpoint);
					diff_eq_int("link callback starts idle window (%ld)", o->idle_timer, 0, endpoint);
					diff_eq_int("link callback count (%ld)", events[1], 2, endpoint);
					diff_eq_int("idle parameter read follows request (%ld)", observations[1][1].write, 0, endpoint);
					diff_eq_int("idle parameter is five (%ld)", observations[1][1].parameter, 5, endpoint);
					diff_eq_int("idle timer assigned after get (%ld)", observations[1][1].idle_timer, 0, endpoint);
					diff_eq_int("wrapper reports link transition (%ld)", result[1], DPSTAT_CHANGEDP, endpoint);
					diff_eq_int("publication precedes request (%ld)", o->info.qc_index, o->menu.menu, endpoint);
					diff_eq_int("previous host index replaced (%ld)", o->info.qc_index, 0, endpoint);
					diff_eq_int("previous host LAPM replaced (%ld)", o->info.qc_lapm, 0, endpoint);
					diff_eq_int("LAPM publication precedes request (%ld)", o->info.qc_lapm, (o->menu.b2 >> 6)&1, endpoint);
				}
				if (o->write && o->parameter == 9 && o->value == 0) {
					expired[endpoint]++;
					diff_eq_int("expiry follows negotiated request (%ld)", linked[endpoint], 1, endpoint);
					diff_eq_int("expiry callback count (%ld)", events[1], 1, endpoint);
					diff_eq_int("expiry returns change request (%ld)", result[1], DPSTAT_CHANGEDP, endpoint);
					diff_eq_int("expiry timer published before callback (%ld)", o->idle_timer, -1, endpoint);
					diff_eq_int("expiry f1c published before callback (%ld)", o->f1c, 0, endpoint);
				}
				observed++;
			}
			if (handshake[1][endpoint]->prev_status >= 0 && handshake[1][endpoint]->prev_status <= V8_LAST_ENUM)
				statuses[endpoint][handshake[1][endpoint]->prev_status]++;
		}
		for (side = 0; side < 2; side++) for (endpoint = 0; endpoint < 2; endpoint++)
			memcpy(air[side][endpoint], out[side][!endpoint], sizeof air[side][endpoint]);
	}
	for (endpoint = 0; endpoint < 2; endpoint++) {
		int s;
		printf("  endpoint %d: statuses", endpoint);
		for (s = 0; s <= V8_LAST_ENUM; s++) if (statuses[endpoint][s]) printf(" %d:%ld", s, statuses[endpoint][s]);
		printf("; link requests %d, expiries %d, first link frame %d/%d, LAPM %d, menu %d\n", linked[endpoint], expired[endpoint], first[endpoint], FRAMES, info[1][endpoint].qc_lapm, info[1][endpoint].qc_index);
		if (dp[1][endpoint]) {
			diff_eq_int("one naturally negotiated link (%ld)", linked[endpoint], 1, endpoint);
			diff_eq_int("one lifecycle idle expiry (%ld)", expired[endpoint], 1, endpoint);
		} else {
			diff_eq_int("public peer negotiated once (%ld)", peer_updated[1][endpoint], 1, endpoint);
			diff_eq_int("public peer retains V32, withdraws V34 (%ld)", cm[1][endpoint].b0 & 0xa0, 0x80, endpoint);
		}
	}
	diff_eq_int("observer overflow (%ld)", overflow, 0, 0);
	printf("  read-only callbacks compared: %ld\n", observed);
	diff_eq_int("expected host observations executed (%ld)", observed, wrapper_endpoint == -1 ? 6 : 3, wrapper_endpoint);
	done:
	for (side = 0; side < 2; side++) for (endpoint = 0; endpoint < 2; endpoint++)
		if (dp[side][endpoint]) ops[side]->destroy(dp[side][endpoint]);
		else if (handshake[side][endpoint]) {
			if (side) ref_V8Delete(handshake[side][endpoint]);
			else V8Delete(handshake[side][endpoint]);
		}
	dp_v8_exit();
	ref_dp_v8_exit();
	rc |= diff_end(); return rc;
}

int main(void)
{
	int rc;
	negative = getenv("V8_HISTORY_NEGATIVE") != 0;
	rc = run_history(-1);
	rc |= run_history(0);
	rc |= run_history(1);
	return rc;
}
