/* Explicit component probe: no planted history/count. Run
 * tools/probe_v90_sd_boundaries.py --period-dir build/period after
 * make period T=t_v90spectral. External threshold/sample quiet NaNs are
 * component inputs, not evidence of a modem lifecycle containing them.
 */
#define main included_spectral_main
#include "../unit/t_v90spectral.cpp"
#undef main

int main()
{
	static const unsigned limits[] = {0, 1, 3, 0x80000000u};
	int mask, limit, sample_mode, step;

	diff_begin("SD constructor threshold/limit/public-history boundary");
	for (mask = 0; mask < 8; ++mask)
	for (limit = 0; limit < 4; ++limit)
	for (sample_mode = 0; sample_mode < 2; ++sample_mode) {
		unsigned char sa[sizeof(V90SdDetector)], sb[sizeof(V90SdDetector)];
		unsigned thresholds[3] = {0x3f800000u, 0x3f000000u, 0xbf000000u};
		int trial = (mask * 4 + limit) * 2 + sample_mode;
		int j;
		for (j = 0; j < 3; ++j)
			if (mask & (1 << j)) thresholds[j] = 0x7fc00000u;
		harness_alloc_reset();
		seed_pair(sd_a, sd_b, SD_SLOT, trial + 900);
		our_sd_ctor(sd_a, thresholds[0], thresholds[1], thresholds[2], limits[limit]);
		ref_sd_ctor(sd_b, thresholds[0], thresholds[1], thresholds[2], limits[limit]);
		for (step = 0; step < 40; ++step) {
			V90SdDetector *a = (V90SdDetector *)sd_a;
			V90SdDetector *b = (V90SdDetector *)sd_b;
			unsigned bits = step < 16 ? 0x3f800000u : 0;
			if (step == 16 && sample_mode) bits = 0x7fc00000u;
			if (step == 24) { a->reset(); ref_sd_reset(b); }
			int ra = our_sd_process(a, bits), rb = ref_sd_process(b, bits);
			diff_eq_int("SD verdict (%ld)", ra, rb, trial * 40 + step);
			sd_snapshot(sa, sd_a); sd_snapshot(sb, sd_b);
			diff_eq_obj_(__FILE__, __LINE__, "SD state", "V90SdDetector",
				     sa, sb, sizeof sa, trial * 40 + step);
			diff_eq_obj_(__FILE__, __LINE__, "SD history", "float",
				     a->history, b->history, SD_HIST_BYTES, trial * 40 + step);
		}
		our_sd_dtor(sd_a); ref_sd_dtor(sd_b);
	}
	return diff_end();
}
