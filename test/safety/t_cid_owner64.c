/* Native owner-allocation boundary. The real CID service constructor and
 * destructor run with an instrumented DTMF endpoint; this is not a received
 * Caller ID message or a complete receiver lifecycle. */
#include <stdio.h>
#include <stdlib.h>
#include "dsplib/cid_modem.h"
#include "dsplib/cid.h"
#include "dsplib/dtmf_rx.h"
#include "dsplib/fpm_mrf.h"

unsigned int dsplibs_debug_level;
int dsplibs_debug_printf(const char *fmt, ...) { (void)fmt; return 0; }
static struct dtmf_rx endpoint;
static unsigned int requested;
static int allocations, root_frees, endpoint_frees, creates, checks, failures;
static void check(const char *name, int ok)
{
	++checks;
	if (!ok) { ++failures; printf("FAIL %s\n", name); }
}
void *sysdep_malloc(unsigned int n)
{
	requested = n;
	++allocations;
	return calloc(1, n);
}
void sysdep_free(void *p)
{
	if (p == &endpoint) ++endpoint_frees;
	else if (p != NULL) { ++root_frees; free(p); }
}
struct dtmf_rx *create_cid_dtmf(struct dtmf_rx *p)
{
	++creates;
	check("existing DTMF owner", p == NULL || p == &endpoint);
	return &endpoint;
}
struct cid *create_cid(struct cid *p) { (void)p; abort(); }
void FPM_MRF_free(struct fpm_mrf *p, int unused)
{ (void)p; (void)unused; abort(); }

int main(void)
{
	struct cid_modem *p, *q;
	check("native LP64 layout", sizeof(void *) == 8);
	p = cid_create(NULL, 73, CID_MODE_DTMF);
	check("owner allocated", p != NULL);
	if (p == NULL) return 1;
	check("request covers native owner", requested == sizeof(*p));
	check("fresh block fill", p->samples_fill == 0);
	check("DTMF endpoint installed", p->dtmf == &endpoint);
	check("initial scalar configuration", p->mode == CID_MODE_DTMF && p->cid_val == 73);
	p->samples_fill = 7;
	q = cid_create(p, 91, CID_MODE_DTMF);
	check("reuse preserves owner", q == p && allocations == 1);
	check("reuse preserves block history", q->samples_fill == 7);
	check("reuse updates configuration", q->cid_val == 91 && creates == 2);
	cid_delete(q);
	check("owner and endpoint released", root_frees == 1 && endpoint_frees == 1);
	printf("CID owner64 safety: %d checks, %d failures\n", checks, failures);
	return failures != 0;
}
