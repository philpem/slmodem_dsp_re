/* Request-boundary differential instrumentation, not modem end-to-end coverage.
 * Real zero-context create/reinit histories produce the requests. Interpose the
 * lowercase adapters, record their first 14 defined template bytes, then call
 * the real upper-case control consumer. Bytes +14 onward include alignment
 * padding and differ across request extents, so are deliberately not inspected.
 * V17 short-training rate codes always select V17; the V27/V29 TX request arms
 * cannot be reached by these valid rate-code histories. They are not
 * instrumented here; no synthetic modem configuration is used.
 */
#include <string.h>
#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1tx.h"
#include "dsplib/class1rx.h"
#include "dsplib/faxvmi.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"

extern void ref__init_transmitter(struct fax_class1 *, int);
extern void ref__init_receiver(struct fax_class1 *, int);

struct request_prefix { unsigned char bytes[14]; };
static struct request_prefix captured[2];
static int calls[2], slots[2], side;

static void capture(const void *arg, int slot)
{
	memcpy(captured[side].bytes, arg, sizeof(captured[side].bytes));
	calls[side]++;
	slots[side] = slot;
}

#define ADAPTER(name, upper, slot, argtype) \
extern int __real_##name(struct faxvmi_link *, argtype); \
extern int ref_##upper(void *, argtype); \
int __wrap_##name(struct faxvmi_link *dp, argtype arg) \
{ capture(arg, slot); return __real_##name(dp, arg); } \
int ref_##name(struct faxvmi_link *dp, argtype arg) \
{ capture(arg, slot); return ref_##upper(dp->modem, arg); }

ADAPTER(v17tx_control, V17TX_control, VMI_SLOT_V17TX, const struct v17tx_control_req *)
ADAPTER(v17rx_control, V17RX_control, VMI_SLOT_V17RX, const struct v17rx_ctl *)
ADAPTER(v29rx_control, V29RX_control, VMI_SLOT_V29RX, const struct v29rx_control_req *)
ADAPTER(v27rx_control, V27RX_control, VMI_SLOT_V27RX, void *)

static void reset_capture(void)
{
	memset(captured, 0, sizeof(captured));
	memset(calls, 0, sizeof(calls));
	memset(slots, 0, sizeof(slots));
}

static void compare_capture(int code, int expected_slot)
{
	diff_eq_int("blob request count", calls[0], 1, code);
	diff_eq_int("source request count", calls[1], 1, code);
	diff_eq_int("blob request slot", slots[0], expected_slot, code);
	diff_eq_int("source request slot", slots[1], expected_slot, code);
	diff_eq_obj("defined request prefix", struct request_prefix,
		    &captured[1], &captured[0], code);
}

int main(void)
{
	static const int tx_codes[] = { 0x4a, 0x62, 0x7a, 0x92 };
	static const int rx_codes[] = { 0x18, 0x30, 0x48, 0x60,
				       0x4a, 0x62, 0x92 };
	int i;
	diff_begin("fax control request boundary: lifecycle-produced prefixes");
	for (i = 0; i < 4; ++i) {
		struct fax_class1 a, b;
		memset(&a, 0, sizeof(a));
		memset(&b, 0, sizeof(b));
		reset_capture();
		side = 0; ref__init_transmitter(&a, tx_codes[i]);
		side = 1; _init_transmitter(&b, tx_codes[i]);
		compare_capture(tx_codes[i], VMI_SLOT_V17TX);
	}
	for (i = 0; i < 7; ++i) {
		struct fax_class1 a, b;
		int slot = i < 2 ? VMI_SLOT_V27RX :
			   i < 4 ? VMI_SLOT_V29RX : VMI_SLOT_V17RX;
		memset(&a, 0, sizeof(a));
		memset(&b, 0, sizeof(b));
		reset_capture();
		side = 0; ref__init_receiver(&a, rx_codes[i]);
		side = 1; _init_receiver(&b, rx_codes[i]);
		diff_eq_int("blob fresh RX request count", calls[0], 0, i);
		diff_eq_int("source fresh RX request count", calls[1], 0, i);
		reset_capture();
		side = 0; ref__init_receiver(&a, rx_codes[i]);
		side = 1; _init_receiver(&b, rx_codes[i]);
		compare_capture(rx_codes[i], slot);
	}
	return diff_end();
}
