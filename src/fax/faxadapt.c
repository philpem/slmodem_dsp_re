/*
 * faxadapt.c -- Class 1 fax: the lowercase per-modulation adapters.
 *
 * See `faxadapt.h` for the address map (F9271), what is written here and
 * what remains blocked and on what.  Functions appear in the OBJECT'S OWN
 * ADDRESS ORDER within each modulation's block, because this is the whole
 * translation unit and register allocation follows emission order (F7796).
 *
 * Differential test: `test/unit/t_faxadapt.c`.
 */

#include <stddef.h>

#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v17fax.h"
#include "dsplib/v21cfg.h"
#include "dsplib/v21fax.h"
#include "dsplib/v27fax.h"
#include "dsplib/v29fax.h"

/* ========================================================================= */
/* V.17 -- 0x09bf20..0x09c29f                                                */
/* ========================================================================= */

/*
 * v17tx_create, 0x09bf20.  `pack_count` is a literal 0x30 on every path --
 * `V17TX_MODEM_BUDGET`, the TX side's own per-call budget, planted here
 * unconditionally rather than derived. `pack_width` is set from
 * `local.bitrate` by a three-way classification (14400 and 12000 each get
 * their own arm, everything else -- including 9600 -- falls to the `else`,
 * which further splits 9600 from the rest) -- the same three-way shape
 * `v17rx_create` already uses for `unpack_width`, one modulation side over,
 * and reproduced here as the object's own literals (6, 5, and 4-or-3) rather
 * than by indexing `V17TX_SYM_SIZE`: `dis.py` over 0x9bf87..0x9bfae shows no
 * reference to that table at all. `unpack_width` is written to 0 on every
 * path -- TX only ever writes `pack_width` meaningfully; see faxadapt.h.
 */
void
v17tx_create(struct faxvmi_link *dp, const struct v17tx_cfg *cfg)
{
	struct v17tx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V17TX_CFG;

	handle = V17TX_create((void *)(long)dp->int_0014, &local);
	dp->pack_count = 0x30;
	dp->int_0014 = (int)(long)handle;

	if (local.bitrate == 14400) {
		dp->pack_width = 6;
		dp->unpack_width = 0;
	} else if (local.bitrate == 12000) {
		dp->pack_width = 5;
		dp->unpack_width = 0;
	} else {
		dp->unpack_width = 0;
		dp->pack_width = (short)((local.bitrate == 9600) ? 4 : 3);
	}
}

/*
 * v17rx_create, 0x09c030.  `dp->int_0014` doubles as the "reinitialise an
 * existing instance" argument V17RX_create takes and the slot its result is
 * stored back into -- D1222 in `t_v17rxcreate.c` is the same contract seen
 * from `V17RX_create`'s own side.  The three branches on `local.bit_rate`
 * are the object's own (0x09c0a5..0x09c0e4): 14400 and 12000 get their own
 * arm, everything else (including 9600) falls to the `else`, which further
 * splits 9600 from the rest.  `pack_width` is always 0 here -- RX only ever
 * writes `unpack_width` (see faxadapt.h).
 */
void
v17rx_create(struct faxvmi_link *dp, const struct v17rx_cfg *cfg)
{
	struct v17rx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V17RX_CFG;

	handle = V17RX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;
	dp->pack_count = 0;

	if (local.bit_rate == 14400) {
		dp->unpack_width = 6;
		dp->pack_width = 0;
	} else if (local.bit_rate == 12000) {
		dp->unpack_width = 5;
		dp->pack_width = 0;
	} else {
		dp->pack_width = 0;
		dp->unpack_width = (local.bit_rate == 9600) ? 4 : 3;
	}
}

/* v17tx_delete, 0x09c160. */
void
v17tx_delete(struct faxvmi_link *dp)
{
	V17TX_delete((void *)(long)dp->int_0014);
}

/* v17rx_delete, 0x09c170. */
void
v17rx_delete(struct faxvmi_link *dp)
{
	V17RX_delete((void *)(long)dp->int_0014);
}

/*
 * v17tx_process, 0x09c180.  `dp->buf` (+0x04) is `V17TX_modem`'s `in`; the
 * caller's `out`/`count` are forwarded straight through and `*result` gets
 * the sample count `V17TX_modem` left in `*count`, which is then cleared --
 * both the store and the clear are the object's own final four instructions
 * (0x09c1aa..0x09c1b8).
 */
void
v17tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
	     unsigned short *result)
{
	V17TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

/*
 * v17rx_process, 0x09c1c0.  The mirror of v17tx_process: `in` is the
 * caller's, `out` is `dp->buf` (cast: V17RX_modem's `out` is `short *`,
 * `dp->buf` is `unsigned short *` -- a pointer reinterpretation, not a
 * narrowing), and (result, count) swap formal positions relative to the TX
 * side (see faxadapt.h).
 */
void
v17rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
	     unsigned short *count)
{
	V17RX_modem((void *)(long)dp->int_0014, in, (short *)dp->buf, count);
	*result = *count;
	*count = 0;
}

/* v17tx_status, 0x09c200. */
int
v17tx_status(struct faxvmi_link *dp, struct v17_status *status)
{
	return V17TX_status((void *)(long)dp->int_0014, status);
}

/* v17rx_status, 0x09c210. */
int
v17rx_status(struct faxvmi_link *dp, struct v17_status *status)
{
	return V17RX_status((void *)(long)dp->int_0014, status);
}

/*
 * v17tx_control, 0x09c220, and v17rx_control, 0x09c230.  BLOCKED: neither
 * V17TX_control nor V17RX_control is written.  Not declared in faxadapt.h.
 */

/*
 * v17tx_message, 0x09c240, and v17rx_message, 0x09c270, are already written
 * in `class1tx.c` (`MESSAGE_FN`), not here.
 */

/* ========================================================================= */
/* V.21 -- 0x09c2a0..0x09c53f                                                */
/* ========================================================================= */

/*
 * v21tx_create, 0x09c2a0.  BLOCKED: V21TX_create and closure (needs 8).
 * Not declared in faxadapt.h.
 */

/*
 * v21rx_create, 0x09c360.  V.21 has one rate (300 bps), so there is no
 * branch: `unpack_width` is the constant 1 on every path (0x09c3b5).
 */
void
v21rx_create(struct faxvmi_link *dp, const struct v21rx_cfg *cfg)
{
	struct v21rx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V21RX_CFG;

	handle = V21RX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;
	dp->pack_count = 0;
	dp->unpack_width = 1;
	dp->pack_width = 0;
}

/* v21tx_delete, 0x09c400. */
void
v21tx_delete(struct faxvmi_link *dp)
{
	V21TX_delete((void *)(long)dp->int_0014);
}

/* v21rx_delete, 0x09c410. */
void
v21rx_delete(struct faxvmi_link *dp)
{
	V21RX_delete((void *)(long)dp->int_0014);
}

/* v21tx_process, 0x09c420.  Same shape as v17tx_process. */
void
v21tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
	     unsigned short *result)
{
	V21TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

/*
 * v21rx_process, 0x09c460.  Same shape as v17rx_process, except
 * `V21RX_modem`'s `count` is `short *` rather than `unsigned short *`; the
 * dereference after the call is still `movzwl` (0x09c48a), so `count` stays
 * declared `unsigned short *` here and is cast only at the call.
 */
void
v21rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
	     unsigned short *count)
{
	V21RX_modem((void *)(long)dp->int_0014, in, (short *)dp->buf,
		    (short *)count);
	*result = *count;
	*count = 0;
}

/* v21tx_status, 0x09c4a0. */
int
v21tx_status(struct faxvmi_link *dp, struct v21_status *status)
{
	return V21TX_status((void *)(long)dp->int_0014, status);
}

/* v21rx_status, 0x09c4b0. */
int
v21rx_status(struct faxvmi_link *dp, struct v21_status *status)
{
	return V21RX_status((void *)(long)dp->int_0014, status);
}

/*
 * v21tx_control, 0x09c4c0, and v21rx_control, 0x09c4d0.  BLOCKED: neither
 * V21TX_control nor V21RX_control is written.  Not declared in faxadapt.h.
 */

/*
 * v21tx_message, 0x09c4e0, and v21rx_message, 0x09c510, are already written
 * in `class1tx.c`, not here.
 */

/* ========================================================================= */
/* V.27ter -- 0x09c540..0x09c83f                                             */
/* ========================================================================= */

/*
 * v27tx_create, 0x09c540.  BLOCKED: V27TX_create and closure (needs 46).
 * Not declared in faxadapt.h.
 */

/*
 * v27rx_create, 0x09c630.  Two rates, 2400 and 4800; unpack_width is 2 for
 * 2400 and 3 otherwise (0x09c68b..0x09c6a4, `setne`+2).
 */
void
v27rx_create(struct faxvmi_link *dp, const struct v27rx_cfg *cfg)
{
	struct v27rx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V27RX_CFG;

	handle = V27RX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;
	dp->pack_count = 0;
	dp->pack_width = 0;
	dp->unpack_width = (local.bit_rate == 2400) ? 2 : 3;
}

/* v27tx_delete, 0x09c700. */
void
v27tx_delete(struct faxvmi_link *dp)
{
	V27TX_delete((void *)(long)dp->int_0014);
}

/* v27rx_delete, 0x09c710. */
void
v27rx_delete(struct faxvmi_link *dp)
{
	V27RX_delete((void *)(long)dp->int_0014);
}

/*
 * v27tx_process, 0x09c720.  BLOCKED: V27TX_modem and V27TX_FRMSIZE are not
 * written -- the one process adapter of the eight ready modulation sides
 * that is not, because it is the only TX modem of the four still unwritten.
 * Not declared in faxadapt.h.
 */

/* v27rx_process, 0x09c760.  Same shape as v17rx_process. */
void
v27rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
	     unsigned short *count)
{
	V27RX_modem((void *)(long)dp->int_0014, in, (short *)dp->buf, count);
	*result = *count;
	*count = 0;
}

/* v27tx_status, 0x09c7a0. */
int
v27tx_status(struct faxvmi_link *dp, void *status)
{
	return V27TX_status((const void *)(long)dp->int_0014, status);
}

/* v27rx_status, 0x09c7b0. */
int
v27rx_status(struct faxvmi_link *dp, void *status)
{
	return V27RX_status((void *)(long)dp->int_0014, status);
}

/*
 * v27tx_control, 0x09c7c0, and v27rx_control, 0x09c7d0.  BLOCKED: neither
 * V27TX_control nor V27RX_control is written.  Not declared in faxadapt.h.
 */

/*
 * v27tx_message, 0x09c7e0, and v27rx_message, 0x09c810, are already written
 * in `class1tx.c`, not here.
 */

/* ========================================================================= */
/* V.29 -- 0x09c840..0x09cafd                                                */
/* ========================================================================= */

/*
 * v29tx_create, 0x09c840.  BLOCKED: V29TX_create and closure (needs 23).
 * Not declared in faxadapt.h.
 */

/*
 * v29rx_create, 0x09c910.  Two rates, 9600 and 7200; unpack_width is 3 for
 * 7200 and 4 otherwise (0x09c967..0x09c97a, `setne`+3).
 */
void
v29rx_create(struct faxvmi_link *dp, const struct v29rx_cfg *cfg)
{
	struct v29rx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V29RX_CFG;

	handle = V29RX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;
	dp->pack_count = 0;
	dp->pack_width = 0;
	dp->unpack_width = (local.bit_rate == 7200) ? 3 : 4;
}

/* v29tx_delete, 0x09c9c0. */
void
v29tx_delete(struct faxvmi_link *dp)
{
	V29TX_delete((void *)(long)dp->int_0014);
}

/* v29rx_delete, 0x09c9d0. */
void
v29rx_delete(struct faxvmi_link *dp)
{
	V29RX_delete((void *)(long)dp->int_0014);
}

/* v29tx_process, 0x09c9e0.  Same shape as v17tx_process. */
void
v29tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
	     unsigned short *result)
{
	V29TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

/* v29rx_process, 0x09ca20.  Same shape as v17rx_process. */
void
v29rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
	     unsigned short *count)
{
	V29RX_modem((void *)(long)dp->int_0014, in, (short *)dp->buf, count);
	*result = *count;
	*count = 0;
}

/* v29tx_status, 0x09ca60. */
int
v29tx_status(struct faxvmi_link *dp, void *status)
{
	return V29TX_status((void *)(long)dp->int_0014, status);
}

/* v29rx_status, 0x09ca70. */
int
v29rx_status(struct faxvmi_link *dp, void *status)
{
	return V29RX_status((void *)(long)dp->int_0014, status);
}

/*
 * v29tx_control, 0x09ca80, and v29rx_control, 0x09ca90.  BLOCKED: neither
 * V29TX_control nor V29RX_control is written.  Not declared in faxadapt.h.
 */

/*
 * v29tx_message, 0x09caa0, and v29rx_message, 0x09cad0, are already written
 * in `class1tx.c`, not here.
 */
