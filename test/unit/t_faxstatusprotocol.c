/* Constructor-to-status component checks for the config's complete word. */
#include <string.h>
#include "harness.h"
#include "dsplib/faxcfg.h"
#include "dsplib/v17fax.h"
#include "dsplib/v29fax.h"

extern void *ref_V17RX_create(void *, const struct v17rx_cfg *);
extern void ref_V17RX_delete(void *);
extern int ref_V17RX_status(void *, struct v17_status *);
extern void *ref_V29RX_create(void *, const struct v29rx_cfg *);
extern void ref_V29RX_delete(void *);
extern int ref_V29RX_status(void *, void *);

int
main(void)
{
	static const int protocols[] = { 0x13570001, 0x24680001 };
	int i;

	diff_begin("fax RX config word through constructor and status");
	for (i = 0; i < 2; i++) {
		struct v17rx_cfg cfg17 = V17RX_CFG;
		struct v29rx_cfg cfg29 = V29RX_CFG;
		struct v17_status a17, b17;
		struct v29_status_prefix a29, b29;
		void *a, *b;
		int ra, rb;

		/* Keep all operational defaults, including the protocol low word. */
		cfg17.protocol = protocols[i];
		a = V17RX_create(0, &cfg17);
		b = ref_V17RX_create(0, &cfg17);
		diff_eq_int("V17 full config word copied, input %ld",
			    ((struct v17rx *)a)->cfg.protocol, protocols[i], i);
		diff_eq_int("V17 reference full config word, input %ld",
			    ((struct v17rx *)b)->cfg.protocol, protocols[i], i);
		memset(&a17, 0x5a, sizeof(a17));
		memset(&b17, 0x5a, sizeof(b17));
		ra = V17RX_status(a, &a17);
		rb = ref_V17RX_status(b, &b17);
		diff_eq_int("V17 status return, input %ld", ra, rb, i);
		diff_eq_int("V17 status report low word, input %ld",
			    a17.protocol, protocols[i] & 0xffff, i);
		diff_eq_obj("V17 fresh receiver report", struct v17_status,
			    &a17, &b17, i);
		V17RX_delete(a);
		ref_V17RX_delete(b);

		cfg29.protocol = protocols[i];
		a = V29RX_create(0, &cfg29);
		b = ref_V29RX_create(0, &cfg29);
		diff_eq_int("V29 full config word copied, input %ld",
			    ((struct v29_rx *)a)->cfg.protocol, protocols[i], i);
		diff_eq_int("V29 reference full config word, input %ld",
			    ((struct v29_rx *)b)->cfg.protocol, protocols[i], i);
		memset(&a29, 0x5a, sizeof(a29));
		memset(&b29, 0x5a, sizeof(b29));
		ra = V29RX_status(a, &a29);
		rb = ref_V29RX_status(b, &b29);
		diff_eq_int("V29 status return, input %ld", ra, rb, i);
		diff_eq_int("V29 status report low word, input %ld",
			    a29.protocol, protocols[i] & 0xffff, i);
		diff_eq_obj("V29 fresh receiver report", struct v29_status_prefix,
			    &a29, &b29, i);
		V29RX_delete(a);
		ref_V29RX_delete(b);
	}
	return diff_end();
}
