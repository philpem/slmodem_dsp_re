/* LP64 component-boundary control: real FAXVMI dispatch and V.21 adapters,
 * stub modem endpoint. This does not claim modem end-to-end reachability. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dsplib/faxvmi.h"
#include "dsplib/v21cfg.h"
#include "dsplib/v21fax.h"

void v21tx_create(struct faxvmi_link *, const struct v21tx_cfg *);
static void *want_modem, *want_req;
static int calls, failures, checks, negative;
static void check(const char *name, int ok)
{
	++checks;
	if (!ok) { ++failures; printf("%s: %s\n", negative ? "expected rejection" : "FAIL", name); }
}
struct v21tx_cfg V21TX_CFG = { 0 };
void *V21TX_create(void *modem, const struct v21tx_cfg *cfg)
{
	check("adapter existing handle", modem == want_modem);
	check("adapter configuration", cfg->protocol == 0);
	return want_req;
}
int V21TX_control(void *modem, const struct v21tx_ctl *req)
{
	++calls;
	check("control modem pointer", modem == want_modem);
	check("control request pointer", req == want_req);
	/* Never dereference an invalid pointer: negative control fails safely. */
	return modem == want_modem && req == want_req ? 73 : -1;
}
int V21RX_control(void *modem, const struct v21rx_ctl *req)
{ (void)modem; (void)req; return -1; }
#define UNUSED_CONTROL(name) \
	int name(struct faxvmi_link *p, void *r) { (void)p; (void)r; return -1; }
UNUSED_CONTROL(v27tx_control)
UNUSED_CONTROL(v27rx_control)
UNUSED_CONTROL(v29tx_control)
UNUSED_CONTROL(v29rx_control)
UNUSED_CONTROL(v17tx_control)
UNUSED_CONTROL(v17rx_control)
/* Other slots are out of this component fixture's input domain. */
#define UNUSED_CREATE(name) \
	void name(struct faxvmi_link *p, const void *r) { (void)p; (void)r; abort(); }
#define UNUSED_DELETE(name) \
	void name(struct faxvmi_link *p) { (void)p; abort(); }
UNUSED_CREATE(v17tx_create)
UNUSED_CREATE(v17rx_create)
UNUSED_CREATE(v27tx_create)
UNUSED_CREATE(v27rx_create)
UNUSED_CREATE(v29tx_create)
UNUSED_CREATE(v29rx_create)
UNUSED_DELETE(v17tx_delete)
UNUSED_DELETE(v17rx_delete)
UNUSED_DELETE(v27tx_delete)
UNUSED_DELETE(v27rx_delete)
UNUSED_DELETE(v29tx_delete)
UNUSED_DELETE(v29rx_delete)
struct v21rx_cfg V21RX_CFG = { 0 };
void *V21RX_create(void *m, const struct v21rx_cfg *cfg)
{ (void)m; (void)cfg; abort(); }
void V21TX_delete(void *m) { (void)m; abort(); }
void V21RX_delete(void *m) { (void)m; abort(); }
const struct faxvmi_cfg FAXVMI_CFG = { 0 };
static int allocations, frees;
void *sysdep_malloc(unsigned int n)
{
	void *p = calloc(1, n ? n : 1);
	if (!p) abort();
	++allocations;
	return p;
}
void sysdep_free(void *p) { ++frees; free(p); }
int main(void)
{
	struct faxvmi vmi;
	struct faxvmi_link link;
	struct faxvmi_framer framer;
	struct faxvmi_ctl ctl;
	struct v21tx_ctl request;
	struct faxvmi_cfg cfg = { 0 };
	struct faxvmi *created;
	int modem, before, saved;
	memset(&vmi, 0, sizeof vmi); memset(&link, 0, sizeof link);
	memset(&framer, 0, sizeof framer); memset(&ctl, 0, sizeof ctl);
	memset(&request, 0, sizeof request);
	want_modem = &modem; want_req = &request;
	check("LP64 required", sizeof(void *) == 8);
	check("modem address exceeds uint32", (uintptr_t)want_modem > UINT32_MAX);
	check("request address exceeds uint32", (uintptr_t)want_req > UINT32_MAX);
	link.modem = want_modem;
	v21tx_create(&link, NULL);
	check("adapter returned handle", link.modem == want_req);
	link.modem = want_modem;
	vmi.link = &link; vmi.framer = &framer; vmi.slot = 5;
	ctl.control_req = want_req;
	check("FAXVMI control result", FAXVMI_control(&vmi, &ctl) == 73);
	check("one endpoint call", calls == 1);
	/* Reproduce the old pointer-in-int truncation; detector must report the
		* endpoint mismatch without dereferencing the truncated value. */
	/* Real allocation/deallocation lifecycle in the null modem slot. */
	cfg.modem_cfg = want_modem;
	cfg.ptr_0014 = want_req;
	cfg.fifo_size = 8; cfg.max_frame = 4; cfg.frame_size = 8;
	created = FAXVMI_create(NULL, &cfg);
	check("create modem_cfg high pointer", created->modem_cfg == want_modem);
	check("create trailing high pointer", created->ptr_0014 == want_req);
	check("create null modem", created->link->modem == NULL);
	check("create initialized buffer", created->link->buf[49] == 0xffff);
	check("create native owner allocations", allocations == 7);
	FAXVMI_delete(created);
	check("delete frees all allocations", frees == allocations);
	negative = 1;
	for (modem = 0; modem < 2; ++modem) {
		ctl.control_req = modem ? want_req : (void *)(uintptr_t)(uint32_t)(uintptr_t)want_req;
		link.modem = modem ? (void *)(uintptr_t)(uint32_t)(uintptr_t)want_modem : want_modem;
		before = failures;
		saved = FAXVMI_control(&vmi, &ctl);
		if (failures != before + 1 || saved != -1) {
			printf("FAIL: truncation negative control did not fire exactly once\n");
			++failures;
		} else failures = before;
		++checks;
	}
	printf("fax pointer64 safety: %d checks, %d failures; 2 truncation negative controls\n", checks, failures);
	return failures != 0;
}
