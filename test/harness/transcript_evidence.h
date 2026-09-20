/* Lossless diagnostic collection, not an exemption or a numeric policy.
 * Length-based hex includes embedded NULs; overflow is a hard failure even
 * when both truncated strings agree. The caller supplies stable site/input IDs.
 */
#ifndef DSPLIB_TRANSCRIPT_EVIDENCE_H
#define DSPLIB_TRANSCRIPT_EVIDENCE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int transcript_exact(const char *site, long input)
{
	unsigned a = dsplib_debug_capture_size(0);
	unsigned b = dsplib_debug_capture_size(1);
	int complete = dsplib_debug_capture_complete(0)
		&& dsplib_debug_capture_complete(1);
	int equal = complete && a == b
		&& dsplib_debug_capture_lines(0) == dsplib_debug_capture_lines(1)
		&& memcmp(dsplib_debug_capture_text(0),
			  dsplib_debug_capture_text(1), a) == 0;
	if (getenv("DSPLIB_TRANSCRIPT_EVIDENCE")) {
		int side;
		const unsigned char *id = (const unsigned char *)site;
		static unsigned sequence;
		fprintf(stderr, "TRANSCRIPT1 %u ", ++sequence);
		while (*id) fprintf(stderr, "%02x", *id++);
		fprintf(stderr, " %ld %d %d", input,
			complete, equal);
		for (side = 0; side < 2; ++side) {
			unsigned i, n = dsplib_debug_capture_size(side);
			const unsigned char *p = (const unsigned char *)
				dsplib_debug_capture_text(side);
			fprintf(stderr, " %u %u ", dsplib_debug_capture_lines(side), n);
			if (!n) fputc('-', stderr);
			for (i = 0; i < n; ++i) fprintf(stderr, "%02x", p[i]);
		}
		fputc('\n', stderr);
	}
	return equal;
}
#endif
