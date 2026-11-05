/* Synthetic component owner/argument-overlap probe on constructor-produced RX.
 * The union gives both typed objects aligned storage. Request flags alias byte
 * one of the HDX control word, so writing that word overwrites the flag byte.
 * This probes native value lifetime, without claiming modem reachability.
 */
#include <string.h>
#include "harness.h"
#include "dsplib/v21fax.h"

extern void *ref_V21RX_create(void *, const struct v21rx_cfg *);
extern void ref_V21RX_delete(void *);
extern int ref_V21RX_control(void *, const struct v21rx_ctl *);

union ctl_alias {
	struct v21rx_ctl request;
	struct {
		unsigned char prefix[12];
		struct v21_rx_hdx hdx;
	} state;
};

typedef char ctl_alias_hdx_at_12[
	__builtin_offsetof(union ctl_alias, state.hdx) == 12 ? 1 : -1];
typedef char ctl_alias_flags_at_13[
	__builtin_offsetof(union ctl_alias, request.flags) == 13 ? 1 : -1];
typedef char ctl_alias_request_in_bounds[
	__builtin_offsetof(union ctl_alias, request) + sizeof(struct v21rx_ctl)
	<= sizeof(union ctl_alias) ? 1 : -1];
typedef char ctl_alias_owner_in_bounds[
	__builtin_offsetof(union ctl_alias, state.hdx) + sizeof(struct v21_rx_hdx)
	<= sizeof(union ctl_alias) ? 1 : -1];
typedef char ctl_alias_owner_aligned[
	__builtin_offsetof(union ctl_alias, state.hdx)
	% __alignof__(struct v21_rx_hdx) == 0 ? 1 : -1];

int
main(void)
{
	struct v21_rx *a, *b;
	struct v21_rx_hdx *saved_a, *saved_b;
	union ctl_alias ua, ub;
	int ra, rb;

	diff_begin("V21 RX synthetic aligned request flags overlap HDX word");
	a = V21RX_create(0, 0);
	b = ref_V21RX_create(0, 0);
	diff_eq_int("both constructed (%ld)", a != 0 && b != 0, 1, 0);
	if (!a || !b)
		return diff_end();

	saved_a = a->hdx;
	saved_b = b->hdx;
	memset(&ua, 0, sizeof ua);
	memset(&ub, 0, sizeof ub);
	ua.state.hdx = *saved_a;
	ub.state.hdx = *saved_b;
	ua.request.int_0004 = 60000;
	ub.request.int_0004 = 60000;
	ua.request.flags = V21RXCTL_SET_HDX_INT0000 | V21RXCTL_REINIT;
	ub.request.flags = ua.request.flags;
	diff_eq_int("request flags relative to owner (%ld)",
		(unsigned char *)&ua.request.flags - (unsigned char *)&ua.state.hdx,
		1, 0);
	a->hdx = &ua.state.hdx;
	b->hdx = &ub.state.hdx;
	rb = ref_V21RX_control(b, &ub.request);
	ra = V21RX_control(a, &ua.request);
	diff_eq_int("control return (%ld)", ra, rb, 0);
	diff_eq_int("reference reinit clears control word (%ld)",
		ub.state.hdx.int_0000, 0, 0);
	diff_eq_int("control word after aliased request (%ld)",
		ua.state.hdx.int_0000, ub.state.hdx.int_0000, 0);
	diff_eq_int("ours retains supplied owner (%ld)",
		a->hdx == &ua.state.hdx, 1, 0);
	diff_eq_int("reference retains supplied owner (%ld)",
		b->hdx == &ub.state.hdx, 1, 0);
	diff_eq_int("reference flag byte overwritten by word (%ld)",
		ub.request.flags, 0, 0);
	diff_eq_int("ours flag byte after control (%ld)",
		ua.request.flags, ub.request.flags, 0);
	diff_eq_int("full copied config value (%ld)",
		a->cfg.int_0008, b->cfg.int_0008, 0);

	/* The supplied stack owners must not reach the heap destructors. */
	a->hdx = saved_a;
	b->hdx = saved_b;
	V21RX_delete(a);
	ref_V21RX_delete(b);
	return diff_end();
}
