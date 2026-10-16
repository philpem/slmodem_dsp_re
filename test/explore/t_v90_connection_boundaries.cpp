/* Explicit period component probe; outside default unit-test discovery.
 * Constructor-created evaluator plus its public update methods, with finite
 * controls and a caller-supplied quiet NaN. No modem-path reachability claim.
 * Run tools/probe_v90_connection_boundaries.py after make period.
 */
#include <string.h>
#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/modem_params.h"
#include "dsplib/V90ConnectionEvaluator.h"

extern "C" {
void source_ctor(void *, void *)
	asm("_ZN22V90ConnectionEvaluatorC1EP13V90Parameters");
void blob_ctor(void *, void *)
	asm("ref__ZN22V90ConnectionEvaluatorC1EP13V90Parameters");
void source_update(void *, float, unsigned)
	asm("_ZN22V90ConnectionEvaluator14updateAvePdsnrEfj");
void blob_update(void *, float, unsigned)
	asm("ref__ZN22V90ConnectionEvaluator14updateAvePdsnrEfj");
int source_evaluate(void *, float)
	asm("_ZN22V90ConnectionEvaluator14evaluatePhase4Ef");
int blob_evaluate(void *, float)
	asm("ref__ZN22V90ConnectionEvaluator14evaluatePhase4Ef");
void source_const(void *, short, float, float, float)
	asm("_ZN22V90ConnectionEvaluator30updateCurrentConstellationDataEsfff");
void blob_const(void *, short, float, float, float)
	asm("ref__ZN22V90ConnectionEvaluator30updateCurrentConstellationDataEsfff");
int source_connection(void *)
	asm("_ZN22V90ConnectionEvaluator18evaluateConnectionEv");
int blob_connection(void *)
	asm("ref__ZN22V90ConnectionEvaluator18evaluateConnectionEv");
extern unsigned ref_dsplibs_debug_level;
}

int
main()
{
	static const unsigned inputs[] = {
		0x3f000000, 0x3f800000, 0x40000000, 0x7fc00000
	};
	unsigned i;

	diff_begin("ConnectionEvaluator constructor/public-update unordered boundaries");
	for (i = 0; i < 4; ++i) {
		_tagModemParameters mp;
		memset(&mp, 0, sizeof mp);
		mp.vpcmRateLimitLow = 28000;
		mp.vpcmRateLimitHigh = 56000;
		V90Parameters p(&mp);
		unsigned char a[sizeof(V90ConnectionEvaluator)];
		unsigned char b[sizeof(V90ConnectionEvaluator)];
		memset(a, 0, sizeof a);
		memset(b, 0, sizeof b);
		p.PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH = 1.0f;
		p.unnamed_45c = 5;
		p.MAX_NOF_V90_RETRAINS = 5;
		p.PHASE4_ERROR_FOR_V34_FALLBACK = 1000.0f;
		p.PDSNR_THRESHOLD_IN_PHASE4 = 1000.0f;
		p.RETRAIN_DETECT_DURATION = 1000;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		source_ctor(a, &p);
		blob_ctor(b, &p);
		source_update(a, 0.0f, 1);
		blob_update(b, 0.0f, 1);
		float ratio;
		memcpy(&ratio, &inputs[i], 4);
		int va = source_evaluate(a, ratio);
		int vb = blob_evaluate(b, ratio);
		diff_eq_int("phase4 verdict (%ld)", va, vb, i);
		diff_eq_int("original finite/unordered outcome (%ld)", vb,
			    i >= 2 ? V90CE_VERDICT_RETRAIN : V90CE_VERDICT_NONE, i);
		diff_eq_obj("constructor/update/phase4 state",
			    V90ConnectionEvaluator, a, b, i);
	}
	for (i = 0; i < 4; ++i) {
		_tagModemParameters mp;
		memset(&mp, 0, sizeof mp);
		mp.vpcmRateLimitLow = 28000;
		mp.vpcmRateLimitHigh = 56000;
		V90Parameters p(&mp);
		unsigned char a[sizeof(V90ConnectionEvaluator)];
		unsigned char b[sizeof(V90ConnectionEvaluator)];
		memset(a, 0, sizeof a);
		memset(b, 0, sizeof b);
		p.ENABLE_RRN_UP = 1;
		p.ENABLE_RRN_DOWN = 0;
		p.RATE_UP_DETECT_DURATION = 1;
		p.MINIMUM_DURATION_IN_DATA_BEFORE_RRN_UP = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		source_ctor(a, &p);
		blob_ctor(b, &p);
		source_const(a, 10, 1.0f, 1000.0f, 1000.0f);
		blob_const(b, 10, 1.0f, 1000.0f, 1000.0f);
		float avg;
		memcpy(&avg, &inputs[i], 4);
		source_update(a, avg, 1);
		blob_update(b, avg, 1);
		int va = source_connection(a);
		int vb = blob_connection(b);
		diff_eq_int("connection verdict (%ld)", va, vb, i);
		diff_eq_int("original rate-up/unordered outcome (%ld)", vb,
			    i == 0 || i == 3 ? V90CE_VERDICT_RRN_UP : V90CE_VERDICT_NONE, i);
		diff_eq_obj("constructor/constellation/update/connection state",
			    V90ConnectionEvaluator, a, b, i);
	}
	return diff_end();
}
