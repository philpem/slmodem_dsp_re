/*
 * v34digital.c -- layout checks for V.34/V.90 digital initialisation.
 *
 * preinitdigital is defined beside preinitV34 in v34shell.c so the period
 * compiler can see both initialiser bodies. The reference embeds those
 * bodies at 0x59760; its exact translation-unit extent remains unsettled
 * because no local symbol anchors this span (findings F177 and F188).
 *
 * The receive and transmit shell views are V34_SHELL_TX apart. Their
 * callback fields hold the role-dependent scrambler/descrambler pair
 * (findings F137 and F178). These assertions retain the measured layout.
 */

#include "dsplib/sysdep.h"
#include "dsplib/v34digital.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34scram.h"
#include "dsplib/v34shell.h"

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  The two contexts' spacing is the load-bearing one.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34DIG_ASSERT(name, type, field, off) \
	typedef char v34dig_off_##name[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V34DIG_ASSERT(wide_accum,  struct v34_shell,  wide_accum,       0x0a08);
V34DIG_ASSERT(feedback_mask,  struct v34_shell,  feedback_mask,       0x0a16);
V34DIG_ASSERT(hist,  struct v34_shell,  hist,       0x0a18);
V34DIG_ASSERT(conv,  struct v34_shell,  conv,       0x0a28);
V34DIG_ASSERT(conv_sr,  struct v34_shell,  conv_sr,       0x0a2c);
V34DIG_ASSERT(prevk, struct v34_shell,  prev_k,     0x0a38);
V34DIG_ASSERT(subframe_count,  struct v34_shell,  subframe_count,       0x0a3c);
V34DIG_ASSERT(t1,    struct v34_shell,  t1,         0x0a48);
V34DIG_ASSERT(t3,    struct v34_shell,  t3,         0x0c48);
V34DIG_ASSERT(latch, struct v34_shell,  latched,    0x0e4c);
V34DIG_ASSERT(cost,  struct v34_shell,  cost,       0x0eac);
V34DIG_ASSERT(trel,  struct v34_shell,  trellis,    0x0ecc);
V34DIG_ASSERT(state, struct v34_shell,  state,      0x12cc);
V34DIG_ASSERT(sidx,  struct v34_shell,  state_idx,  0x144c);

/*
 * The transmit context is exactly V34_SHELL_TX past the receive one, which
 * `getFrame` established (finding F137) and this function arrives at
 * independently: its second block is at obj+0x25e0 and its first at
 * obj+0xa00.
 */
typedef char v34dig_tx_spacing[((0x25e0 - 0xa00) == V34_SHELL_TX) ? 1 : -1];

/* And the three memsets must be exactly the arrays, not approximately. */
typedef char v34dig_memsets[
	(sizeof(((struct v34_shell *)0)->cost) == 0x20
	 && sizeof(((struct v34_shell *)0)->trellis) == 0x400
	 && sizeof(((struct v34_shell *)0)->state) == 0x180) ? 1 : -1];

#endif
