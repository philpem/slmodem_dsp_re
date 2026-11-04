/*
 * The public int slot is narrowed to a signed word inside MRF. With 65535
 * allocated inputs, its low word is -1: the blob's partial-input countdown
 * wraps through all 65535 inputs and produces no output. Constructor-created
 * state keeps every access within an eight-word ring. This is an initialized
 * component boundary, without a claim of modem lifecycle reachability.
 *
 * Use an int reference alias to preserve the caller's complete input slot.
 * The original formal width remains nonunique. Both output sentinels must
 * remain untouched. The pre-F12124 source fails 9 of these 24 checks.
 */
#include "harness.h"
#include "dsplib/fpm_mrf.h"

#ifndef DSPLIB_REPRODUCE_BUGS
#error "This differential fixture requires bug-for-bug reconstruction mode"
#endif

extern void ref_FPM_MRF_init(void *, const void *, int);
extern void ref_FPM_MRF_free(void *);
extern unsigned int ref_FPM_MRF_filter(void *, const short *, short *, int);

#define INPUT_COUNT 65535
static short input[INPUT_COUNT];

int
main(void)
{
	const short coeff[8] = {0};
	struct fpm_mrf_cfg cfg = {1, 1, coeff, 8, 0, 0};
	struct fpm_mrf ours, blob, normalized_ours, normalized_blob;
	short ours_out = 0x1234, blob_out = 0x1234;
	unsigned int got, want;
	int i, rc;

	for (i = 0; i < INPUT_COUNT; i++)
		input[i] = (short)(i % 30001 - 15000);
	memset(&ours, 0, sizeof(ours));
	memset(&blob, 0, sizeof(blob));
	FPM_MRF_init(&ours, &cfg, 1);
	ref_FPM_MRF_init(&blob, &cfg, 1);

	diff_begin("MRF initialized high input slot");
	got = FPM_MRF_filter(&ours, input, &ours_out, INPUT_COUNT);
	want = ref_FPM_MRF_filter(&blob, input, &blob_out, INPUT_COUNT);
	diff_eq_int("blob output count", want, 0, INPUT_COUNT);
	diff_eq_int("blob need", blob.need, 2, INPUT_COUNT);
	diff_eq_int("blob write index", blob.widx, 7, INPUT_COUNT);
	diff_eq_int("blob phase", blob.phase, 0, INPUT_COUNT);
	diff_eq_int("return", got, want, INPUT_COUNT);
	diff_eq_int("untouched output", ours_out, blob_out, INPUT_COUNT);
	diff_eq_int("blob output sentinel", blob_out, 0x1234, INPUT_COUNT);
	normalized_ours = ours;
	normalized_blob = blob;
	normalized_ours.history = normalized_blob.history = 0;
	diff_eq_obj("state", struct fpm_mrf, &normalized_ours,
		    &normalized_blob, INPUT_COUNT);
	for (i = 0; i < 8; i++) {
		diff_eq_int("blob tail history", blob.history[i],
			    input[INPUT_COUNT - 8 + i], i);
		diff_eq_int("history", ours.history[i], blob.history[i], i);
	}
	rc = diff_end();
	FPM_MRF_free(&ours, 1);
	ref_FPM_MRF_free(&blob);
	printf("MRF input slot: 1 paired initialized call; "
	       "65535 bounded inputs; 0 outputs\n");
	return rc;
}
