/**
 * @file faxadapt.h
 * @brief Class 1 fax: the lowercase per-modulation adapters.
 *
 * `.text` 0x09bf20..0x09cafd is 48 FUNC symbols, no foreign symbol anywhere
 * in the range, laid out as four identical twelve-function blocks in one
 * identical order (finding F9271):
 *
 *     tx_create  rx_create  tx_delete  rx_delete  tx_process  rx_process
 *     tx_status  rx_status  tx_control rx_control tx_message  rx_message
 *
 * repeated for V.17 at 0x09bf20, V.21 at 0x09c2a0, V.27 at 0x09c540 and
 * V.29 at 0x09c840. That makes them one translation unit and this is it --
 * see F9271 for why splitting them by modulation would cost byte identity
 * (register allocation follows the TU's emission order, F7796).
 *
 * The eight message functions are not here. `v??tx_message`/`v??rx_message`
 * for all four modulations were already written in `class1tx.c` (finding
 * F8320's leaf pass, `MESSAGE_FN`) before this file existed, so 8 of the 48
 * are elsewhere and this header does not redeclare them; `class1tx.h` does.
 *
 * What `dp` is: every adapter's first argument is `struct faxvmi_link *`
 * (`faxvmi.h`) -- the 24-byte handle `FAXVMI_create` allocates and hands to
 * `vxx_create[slot]` as its first argument, and to message/status/control
 * as their whole first argument. Two of its fields are what these adapters
 * write:
 *
 *   +0x0c `pack_count`     the TX side's per-call sample count (V.17: 0x30
 *                          always; V.21/V.27/V.29 leave it 0)
 *   +0x0e `pack_width`     the TX side's bits-per-symbol, by rate
 *   +0x10 `unpack_width`   the RX side's bits-per-symbol, by rate
 *   +0x14 `int_0014`       the wrapped vxx handle: read as the "existing
 *                          instance" argument to V??_create (0 on a fresh
 *                          `dp`) and overwritten with what it returns
 *
 * `pack_width`/`unpack_width` is not a guess: FAXVMI's own comment records
 * that the packers read `pack_width` at +0x0e and the unpackers read
 * `unpack_width` at +0x10, and every TX create here writes only the first,
 * every RX create only the second -- the split is exactly the one the field
 * names already predict (evidence class 2, a struct whose own header types
 * the site).
 *
 * `int_0014` is cast through `long`, not directly, because `faxvmi.h` (a
 * different file, another strand of this wave) spells the field `int` and
 * this file does not rename it -- CLAUDE.md's "one type, one home" cuts the
 * other way here: the type is somebody else's live modelling and not this
 * file's to correct. `(void *)(long)` avoids a `-Wint-to-pointer-cast`
 * warning under `make check64` at the cost of being exactly as lossy on a
 * 64-bit build as the field it reads -- a pre-existing property of
 * `int_0014`'s own type, not something introduced here.
 *
 * Status, address order (F9271's map): all 40 non-message symbols are now
 * written. The last three -- `v17tx_control`, `v29tx_control`,
 * `v29rx_control` -- were blocked on their own matching `V??_control`
 * (`v17.c`/`v29.c`); `V17TX_control`/`V29TX_control` landed together
 * (F10107), which is also the pass that noticed `V29RX_control` itself had
 * already landed in wave 9 (F10103) while this comment and the "BLOCKED"
 * note beside `v29rx_control` in `faxadapt.c` both still called it
 * unwritten. `init_vmi_v17tx`/`init_vmi_v27tx`/`init_vmi_v29tx` (F9271's
 * "check with nm -S" question, F10010) are not in this file and never were
 * candidates for it: their addresses (0x94870, 0x94a70, 0x94970) fall in
 * the `class1tx.c` span (0x94870..0xac960), immediately after the
 * `class1rx.c` span (0x93e80..0x94870) that holds their RX siblings
 * `init_vmi_v17rx`/`init_vmi_v27rx`/`init_vmi_v29rx` -- adjacent spans, not
 * the same one, and neither is this file's own range (0x9bf20..0x9cafd).
 * `readyqueue.py --span 'class1tx.c +94'` already attributes all three to
 * `class1tx.c`, which belongs to a different agent's scope, not this file.
 */

#ifndef DSPLIB_FAXADAPT_H
#define DSPLIB_FAXADAPT_H

struct faxvmi_link;
struct v17tx_cfg;
struct v17rx_cfg;
struct v21tx_cfg;
struct v21rx_cfg;
struct v27tx_cfg;
struct v27rx_cfg;
struct v29tx_cfg;
struct v29rx_cfg;
struct v17_status;
struct v21_status;
struct v17rx_ctl;
struct v17tx_control_req;
struct v21tx_ctl;
struct v21rx_ctl;
struct v29tx_control_req;
struct v29rx_control_req;

/*
 * ---- create --------------------------------------------------------------
 *
 * Every `*_create` copies `*cfg` (or the modulation's own default table when
 * `cfg` is NULL) into a local, calls the matching `V??[TR]X_create` with
 * `dp->int_0014` as the "existing instance" handle, stores the result back
 * into `dp->int_0014`, and sets `dp->pack_count`/`pack_width`/`unpack_width`
 * from the local's bit rate. TX writes `pack_count`/`pack_width` and zeroes
 * `unpack_width`; RX writes `unpack_width` and zeroes `pack_width` (and
 * `pack_count`). Where a modulation has more than one rate, the width (and,
 * for V.27ter, the TX side's `pack_count` too) is chosen by a rate compare
 * against the object's own literals, reproduced exactly rather than derived
 * from a lookup table (`dis.py` shows none referenced at these sites).
 */

/** @brief V.17 transmit adapter constructor.
 *
 * `pack_count` is always 0x30. `pack_width` is 6 at 14400 bps, 5 at 12000,
 * 4 at 9600, else 3.
 *
 * @param dp   The wrapped modulation's link handle.
 * @param cfg  Configuration, or NULL for #V17TX_CFG.
 */
void v17tx_create(struct faxvmi_link *dp, const struct v17tx_cfg *cfg);

/** @brief V.17 receive adapter constructor.
 *
 * `unpack_width` is 6 at 14400 bps, 5 at 12000, 4 at 9600, else 3.
 *
 * @param dp   The wrapped modulation's link handle.
 * @param cfg  Configuration, or NULL for #V17RX_CFG.
 */
void v17rx_create(struct faxvmi_link *dp, const struct v17rx_cfg *cfg);

/** @brief V.21 transmit adapter constructor.
 *
 * V.21 has one rate, so `pack_count`/`pack_width` are the unconditional
 * literals 6 and 1.
 *
 * @param dp   The wrapped modulation's link handle.
 * @param cfg  Configuration, or NULL for #V21TX_CFG.
 */
void v21tx_create(struct faxvmi_link *dp, const struct v21tx_cfg *cfg);

/** @brief V.21 receive adapter constructor.
 *
 * V.21 has one rate, so `unpack_width` is the unconditional literal 1.
 *
 * @param dp   The wrapped modulation's link handle.
 * @param cfg  Configuration, or NULL for #V21RX_CFG.
 */
void v21rx_create(struct faxvmi_link *dp, const struct v21rx_cfg *cfg);

/** @brief V.27ter transmit adapter constructor.
 *
 * At 2400 bps `pack_count` is 24 and `pack_width` is 2; otherwise (4800)
 * they are 32 and 3.
 *
 * @param dp   The wrapped modulation's link handle.
 * @param cfg  Configuration, or NULL for #V27TX_CFG.
 */
void v27tx_create(struct faxvmi_link *dp, const struct v27tx_cfg *cfg);

/** @brief V.27ter receive adapter constructor.
 *
 * `unpack_width` is 2 at 2400 bps, else (4800) 3.
 *
 * @param dp   The wrapped modulation's link handle.
 * @param cfg  Configuration, or NULL for #V27RX_CFG.
 */
void v27rx_create(struct faxvmi_link *dp, const struct v27rx_cfg *cfg);

/** @brief V.29 transmit adapter constructor.
 *
 * `pack_count` is always 0x30. `pack_width` is 3 at 7200 bps, else
 * (9600) 4.
 *
 * @param dp   The wrapped modulation's link handle.
 * @param cfg  Configuration, or NULL for #V29TX_CFG.
 */
void v29tx_create(struct faxvmi_link *dp, const struct v29tx_cfg *cfg);

/** @brief V.29 receive adapter constructor.
 *
 * `unpack_width` is 3 at 7200 bps, else (9600) 4.
 *
 * @param dp   The wrapped modulation's link handle.
 * @param cfg  Configuration, or NULL for #V29RX_CFG.
 */
void v29rx_create(struct faxvmi_link *dp, const struct v29rx_cfg *cfg);

/*
 * ---- delete ---------------------------------------------------------------
 *
 * Every `*_delete` is `V??_delete(dp->int_0014)`, tail-called in the object.
 */

/** @brief Tear down a V.17 transmit adapter. @param dp  The link handle. */
void v17tx_delete(struct faxvmi_link *dp);
/** @brief Tear down a V.17 receive adapter. @param dp  The link handle. */
void v17rx_delete(struct faxvmi_link *dp);
/** @brief Tear down a V.21 transmit adapter. @param dp  The link handle. */
void v21tx_delete(struct faxvmi_link *dp);
/** @brief Tear down a V.21 receive adapter. @param dp  The link handle. */
void v21rx_delete(struct faxvmi_link *dp);
/** @brief Tear down a V.27ter transmit adapter. @param dp  The link handle. */
void v27tx_delete(struct faxvmi_link *dp);
/** @brief Tear down a V.27ter receive adapter. @param dp  The link handle. */
void v27rx_delete(struct faxvmi_link *dp);
/** @brief Tear down a V.29 transmit adapter. @param dp  The link handle. */
void v29tx_delete(struct faxvmi_link *dp);
/** @brief Tear down a V.29 receive adapter. @param dp  The link handle. */
void v29rx_delete(struct faxvmi_link *dp);

/*
 * ---- status -----------------------------------------------------------
 *
 * Every `*_status` is `V??_status(dp->int_0014, status)`, tail-called.
 */

/** @brief Report V.17 transmit status.
 * @param dp      The link handle.
 * @param status  Destination for the status record.
 * @return The wrapped `V17TX_status`'s own return.
 */
int v17tx_status(struct faxvmi_link *dp, struct v17_status *status);
/** @brief Report V.17 receive status.
 * @param dp      The link handle.
 * @param status  Destination for the status record.
 * @return The wrapped `V17RX_status`'s own return.
 */
int v17rx_status(struct faxvmi_link *dp, struct v17_status *status);
/** @brief Report V.21 transmit status.
 * @param dp      The link handle.
 * @param status  Destination for the status record.
 * @return The wrapped `V21TX_status`'s own return.
 */
int v21tx_status(struct faxvmi_link *dp, struct v21_status *status);
/** @brief Report V.21 receive status.
 * @param dp      The link handle.
 * @param status  Destination for the status record.
 * @return The wrapped `V21RX_status`'s own return.
 */
int v21rx_status(struct faxvmi_link *dp, struct v21_status *status);
/** @brief Report V.27ter transmit status.
 * @param dp      The link handle.
 * @param status  Destination for the status record (untyped: `v27fax.h`).
 * @return The wrapped `V27TX_status`'s own return.
 */
int v27tx_status(struct faxvmi_link *dp, void *status);
/** @brief Report V.27ter receive status.
 * @param dp      The link handle.
 * @param status  Destination for the status record (untyped: `v27fax.h`).
 * @return The wrapped `V27RX_status`'s own return.
 */
int v27rx_status(struct faxvmi_link *dp, void *status);
/** @brief Report V.29 transmit status.
 * @param dp      The link handle.
 * @param status  Destination for the status record (untyped).
 * @return The wrapped `V29TX_status`'s own return.
 */
int v29tx_status(struct faxvmi_link *dp, void *status);
/** @brief Report V.29 receive status.
 * @param dp      The link handle.
 * @param status  Destination for the status record (untyped).
 * @return The wrapped `V29RX_status`'s own return.
 */
int v29rx_status(struct faxvmi_link *dp, void *status);

/*
 * ---- process: drive one block through `V??_modem` and relay the count ----
 *
 * The TX and RX shapes are mirrored, not the same shape twice. Both read
 * `dp->int_0014` as the handle and both zero a caller `count` after copying
 * it to a caller `result`, but which formal is `in`/`out` and which pair is
 * (result, count) vs (count, result) differs by side -- read directly off
 * `dis.py`, not assumed:
 *
 *   TX: V??TX_modem(handle, dp->buf, out, count); *result = *count; *count=0;
 *   RX: V??RX_modem(handle, in, dp->buf, count);  *result = *count; *count=0;
 *
 * so TX's own second argument is `out` and RX's is `in`, and the count
 * pointer sits at formal index 2 for TX but index 3 for RX. `v27tx_process`
 * is the one blocked member (`V27TX_modem` unwritten) and is not declared.
 */

/** @brief Drive one block through the wrapped V.17 transmit modem.
 * @param dp      The link handle.
 * @param out     Modulated samples out.
 * @param count   In/out: samples requested; zeroed once relayed to `result`.
 * @param result  Out: the sample count `V17TX_modem` produced.
 */
void v17tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		   unsigned short *result);
/** @brief Drive one block through the wrapped V.17 receive modem.
 * @param dp      The link handle.
 * @param in      Received samples in.
 * @param result  Out: the sample count `V17RX_modem` produced.
 * @param count   In/out: samples offered; zeroed once relayed to `result`.
 */
void v17rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		   unsigned short *count);
/** @brief Drive one block through the wrapped V.21 transmit modem.
 * @param dp      The link handle.
 * @param out     Modulated samples out.
 * @param count   In/out: samples requested; zeroed once relayed to `result`.
 * @param result  Out: the sample count `V21TX_modem` produced.
 */
void v21tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		   unsigned short *result);
/** @brief Drive one block through the wrapped V.21 receive modem.
 * @param dp      The link handle.
 * @param in      Received samples in.
 * @param result  Out: the sample count `V21RX_modem` produced.
 * @param count   In/out: samples offered; zeroed once relayed to `result`.
 */
void v21rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		   unsigned short *count);
/**
 * @brief Drive one block through the wrapped V.27ter transmit modem.
 *
 * Not declared: `V27TX_modem` is unwritten, so this member of the
 * F9271 block is blocked.
 *
 * @param dp      The link handle.
 * @param out     Modulated samples out.
 * @param count   In/out: samples requested; zeroed once relayed to `result`.
 * @param result  Out: the sample count `V27TX_modem` would produce.
 */
void v27tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		   unsigned short *result);
/** @brief Drive one block through the wrapped V.27ter receive modem.
 * @param dp      The link handle.
 * @param in      Received samples in.
 * @param result  Out: the sample count `V27RX_modem` produced.
 * @param count   In/out: samples offered; zeroed once relayed to `result`.
 */
void v27rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		   unsigned short *count);
/** @brief Drive one block through the wrapped V.29 transmit modem.
 * @param dp      The link handle.
 * @param out     Modulated samples out.
 * @param count   In/out: samples requested; zeroed once relayed to `result`.
 * @param result  Out: the sample count `V29TX_modem` produced.
 */
void v29tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		   unsigned short *result);
/** @brief Drive one block through the wrapped V.29 receive modem.
 * @param dp      The link handle.
 * @param in      Received samples in.
 * @param result  Out: the sample count `V29RX_modem` produced.
 * @param count   In/out: samples offered; zeroed once relayed to `result`.
 */
void v29rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		   unsigned short *count);

/*
 * ---- control: `V??_control(dp->int_0014, arg)`, tail-called --------------
 *
 * All twelve are now written -- `v17tx_control`/`v29tx_control`/
 * `v29rx_control` (F10107) were the last three, unblocked by
 * `V17TX_control`/`V29TX_control` landing and by re-checking `V29RX_control`
 * (wave 9, F10103) against the stale "BLOCKED" comment this header and
 * `faxadapt.c` both still carried for it. `V27TX_control`/`V27RX_control`
 * take an untyped `void *`, not a struct pointer (v27fax.h).
 */

/** @brief Send a control request to the wrapped V.17 transmit modem.
 * @param dp   The link handle.
 * @param arg  The request.
 * @return The wrapped `V17TX_control`'s own return.
 */
int v17tx_control(struct faxvmi_link *dp,
		  const struct v17tx_control_req *arg);
/** @brief Send a control request to the wrapped V.17 receive modem.
 * @param dp   The link handle.
 * @param arg  The request.
 * @return The wrapped `V17RX_control`'s own return.
 */
int v17rx_control(struct faxvmi_link *dp, const struct v17rx_ctl *arg);
/** @brief Send a control request to the wrapped V.21 transmit modem.
 * @param dp   The link handle.
 * @param arg  The request.
 * @return The wrapped `V21TX_control`'s own return.
 */
int v21tx_control(struct faxvmi_link *dp, const struct v21tx_ctl *arg);
/** @brief Send a control request to the wrapped V.21 receive modem.
 * @param dp   The link handle.
 * @param arg  The request.
 * @return The wrapped `V21RX_control`'s own return.
 */
int v21rx_control(struct faxvmi_link *dp, const struct v21rx_ctl *arg);
/** @brief Send a control request to the wrapped V.27ter transmit modem.
 * @param dp   The link handle.
 * @param req  The request (untyped: `v27fax.h`).
 * @return The wrapped `V27TX_control`'s own return.
 */
int v27tx_control(struct faxvmi_link *dp, void *req);
/** @brief Send a control request to the wrapped V.27ter receive modem.
 * @param dp   The link handle.
 * @param req  The request (untyped: `v27fax.h`).
 * @return The wrapped `V27RX_control`'s own return.
 */
int v27rx_control(struct faxvmi_link *dp, void *req);
/** @brief Send a control request to the wrapped V.29 transmit modem.
 * @param dp   The link handle.
 * @param arg  The request.
 * @return The wrapped `V29TX_control`'s own return.
 */
int v29tx_control(struct faxvmi_link *dp,
		  const struct v29tx_control_req *arg);
/** @brief Send a control request to the wrapped V.29 receive modem.
 * @param dp   The link handle.
 * @param arg  The request.
 * @return The wrapped `V29RX_control`'s own return.
 */
int v29rx_control(struct faxvmi_link *dp,
		  const struct v29rx_control_req *arg);

#endif /* DSPLIB_FAXADAPT_H */
