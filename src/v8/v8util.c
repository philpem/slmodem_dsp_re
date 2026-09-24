/*
 * v8util.c -- the compile-time offset assertions.
 *
 * Every function this file used to carry has found its object unit: the
 * arithmetic and buffer leaves are `V8global.c`'s, the cosine table and DFT
 * energy pass `V8Dftc.c`'s, the V.21 setup `V8Dpsk.c`'s, and v8_TONEq_init
 * is `V8.c`'s, which is the unit that owns its address in the object.  What
 * remains is apparatus and emits no code: the struct-offset assertions
 * below, which make a careless struct edit fail the build instead of a test
 * somewhere far away.  It is an ours-only translation unit.
 */

#include <stddef.h>

#include "dsplib/v8.h"

/*
 * The offsets are the whole point of this file, so they are checked at
 * compile time rather than trusted.  Every number below was read out of the
 * object; if a struct is edited carelessly the build stops here instead of a
 * test failing somewhere far away.
 */
/*
 * Only on a 32-bit build.  The object holds pointers at fixed offsets, so its
 * layout is a property of the original's ABI and cannot hold where a pointer
 * is eight bytes.  `make check64` compiles this file for the host purely to
 * keep the C portable, and these numbers are not portable by construction.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V8_ASSERT_OFFSET(s, m, off) \
	typedef char v8_off_##m[offsetof(struct s, m) == (off) ? 1 : -1]
#else
#define V8_ASSERT_OFFSET(s, m, off) \
	typedef char v8_off_##m[1]
#define V8_OFFSETS_UNCHECKED	1
#endif

#ifdef V8_OFFSETS_UNCHECKED
#define V8_OFFSET_OK	0
#else
#define V8_OFFSET_OK	1
#endif

V8_ASSERT_OFFSET(v8, rx, 0x01c);
V8_ASSERT_OFFSET(v8, sym_avail, 0x110);
V8_ASSERT_OFFSET(v8, tx_symbols, 0x11c);
V8_ASSERT_OFFSET(v8, tx_avail, 0x21c);
V8_ASSERT_OFFSET(v8, tx_ring, 0x228);
V8_ASSERT_OFFSET(v8, rx_stage, 0x5c8);
V8_ASSERT_OFFSET(v8, tx_stage, 0x5c0);
V8_ASSERT_OFFSET(v8, tx_shape, 0x77c);
V8_ASSERT_OFFSET(v8, rx_scratch, 0x894);
V8_ASSERT_OFFSET(v8, tx_gain, 0xa42);
V8_ASSERT_OFFSET(v8, cm, 0xa58);
V8_ASSERT_OFFSET(v8, v21_taps, 0xa5c);
V8_ASSERT_OFFSET(v8, detector, 0xad8);
V8_ASSERT_OFFSET(v8, v21_params, 0xc20);
V8_ASSERT_OFFSET(v8, tx_seq, 0xc48);
V8_ASSERT_OFFSET(v8, seq, 0xc54);
V8_ASSERT_OFFSET(v8, tone, 0xda4);
V8_ASSERT_OFFSET(v8, word_count, 0xdbc);
V8_ASSERT_OFFSET(v8, quick_connect, 0xdc4);
V8_ASSERT_OFFSET(v8, deadline_a, 0xe5c);
V8_ASSERT_OFFSET(v8, agc_line, 0xe68);
V8_ASSERT_OFFSET(v8, fn_matched, 0xebc);
V8_ASSERT_OFFSET(v8, side, 0xa44);
V8_ASSERT_OFFSET(v8, phase_rev, 0xb40);
typedef char v8_pr_det[V8_OFFSET_OK == 0
			|| offsetof(struct v8_phase_rev, detected) == 0xdc ? 1 : -1];
V8_ASSERT_OFFSET(v8, v21, 0xdd8);
V8_ASSERT_OFFSET(v8, toneq_pending, 0xdd2);
typedef char v8_size_check[V8_OFFSET_OK == 0 || sizeof(struct v8) == V8_STATE_BYTES ? 1 : -1];

typedef char v8_v21_delay[V8_OFFSET_OK == 0 || offsetof(struct v8, v21)
			  + offsetof(struct v8_v21, delay) == 0xe0c ? 1 : -1];
typedef char v8_pr_window[V8_OFFSET_OK == 0 || offsetof(struct v8_phase_rev, window) == 0x14
			  ? 1 : -1];
