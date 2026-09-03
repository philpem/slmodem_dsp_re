/*
 * faxadapt.h -- Class 1 fax: the lowercase per-modulation adapters.
 *
 * `.text` 0x09bf20..0x09cafd is 48 FUNC symbols, no foreign symbol anywhere
 * in the range, laid out as FOUR IDENTICAL TWELVE-FUNCTION BLOCKS in one
 * identical order (finding F9271):
 *
 *     tx_create  rx_create  tx_delete  rx_delete  tx_process  rx_process
 *     tx_status  rx_status  tx_control rx_control tx_message  rx_message
 *
 * repeated for V.17 at 0x09bf20, V.21 at 0x09c2a0, V.27 at 0x09c540 and V.29
 * at 0x09c840.  That makes them ONE translation unit and this is it -- see
 * F9271 for why splitting them by modulation would cost byte identity
 * (register allocation follows the TU's emission order, F7796).
 *
 * THE EIGHT MESSAGE FUNCTIONS ARE NOT HERE.  `v??tx_message`/`v??rx_message`
 * for all four modulations were already written in `class1tx.c` (finding
 * F8320's leaf pass, `MESSAGE_FN`) before this file existed, so 8 of the 48
 * are elsewhere and this header does not redeclare them; `class1tx.h` does.
 *
 * WHAT "dp" IS.  Every adapter's first argument is `struct faxvmi_link *`
 * (`faxvmi.h`) -- the 24-byte handle `FAXVMI_create` allocates and hands to
 * `vxx_create[slot]` as its FIRST argument, and to message/status/control as
 * their whole first argument.  Two of its fields are what these adapters
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
 * that the PACKERS read `pack_width` at +0x0e and the UNPACKERS read
 * `unpack_width` at +0x10, and every TX create here writes only the first,
 * every RX create only the second -- the split is exactly the one the field
 * names already predict (evidence class 2, a struct whose OWN header types
 * the site).
 *
 * `int_0014` IS CAST THROUGH `long`, NOT DIRECTLY, because `faxvmi.h` (a
 * different file, another strand of this wave) spells the field `int` and
 * this file does not rename it -- CLAUDE.md's "one type, one home" cuts the
 * other way here: the type is somebody else's live modelling and not this
 * file's to correct.  `(void *)(long)` avoids a `-Wint-to-pointer-cast`
 * warning under `make check64` at the cost of being exactly as lossy on a
 * 64-bit build as the field it reads -- a pre-existing property of
 * `int_0014`'s own type, not something introduced here.
 *
 * WHAT IS WRITTEN, AND WHAT IS NOT (F9271's map, address order):
 *
 *   0x09bf20 v17tx_create   WRITTEN (F9910 landed the closure whole)
 *   0x09c030 v17rx_create   WRITTEN
 *   0x09c160 v17tx_delete   WRITTEN
 *   0x09c170 v17rx_delete   WRITTEN
 *   0x09c180 v17tx_process  WRITTEN
 *   0x09c1c0 v17rx_process  WRITTEN
 *   0x09c200 v17tx_status   WRITTEN
 *   0x09c210 v17rx_status   WRITTEN
 *   0x09c220 v17tx_control  WRITTEN (F10107)
 *   0x09c230 v17rx_control  WRITTEN (F10010)
 *   0x09c240 v17tx_message  already in class1tx.c
 *   0x09c270 v17rx_message  already in class1tx.c
 *
 *   0x09c2a0 v21tx_create   WRITTEN (F10010)
 *   0x09c360 v21rx_create   WRITTEN
 *   0x09c400 v21tx_delete   WRITTEN
 *   0x09c410 v21rx_delete   WRITTEN
 *   0x09c420 v21tx_process  WRITTEN
 *   0x09c460 v21rx_process  WRITTEN
 *   0x09c4a0 v21tx_status   WRITTEN
 *   0x09c4b0 v21rx_status   WRITTEN
 *   0x09c4c0 v21tx_control  WRITTEN (F10010)
 *   0x09c4d0 v21rx_control  WRITTEN (F10010)
 *   0x09c4e0 v21tx_message  already in class1tx.c
 *   0x09c510 v21rx_message  already in class1tx.c
 *
 *   0x09c540 v27tx_create   WRITTEN (F10010)
 *   0x09c630 v27rx_create   WRITTEN
 *   0x09c700 v27tx_delete   WRITTEN
 *   0x09c710 v27rx_delete   WRITTEN
 *   0x09c720 v27tx_process  WRITTEN (F10010)
 *   0x09c760 v27rx_process  WRITTEN
 *   0x09c7a0 v27tx_status   WRITTEN
 *   0x09c7b0 v27rx_status   WRITTEN
 *   0x09c7c0 v27tx_control  WRITTEN (F10010)
 *   0x09c7d0 v27rx_control  WRITTEN (F10010)
 *   0x09c7e0 v27tx_message  already in class1tx.c
 *   0x09c810 v27rx_message  already in class1tx.c
 *
 *   0x09c840 v29tx_create   WRITTEN (F10010)
 *   0x09c910 v29rx_create   WRITTEN
 *   0x09c9c0 v29tx_delete   WRITTEN
 *   0x09c9d0 v29rx_delete   WRITTEN
 *   0x09c9e0 v29tx_process  WRITTEN
 *   0x09ca20 v29rx_process  WRITTEN
 *   0x09ca60 v29tx_status   WRITTEN
 *   0x09ca70 v29rx_status   WRITTEN
 *   0x09ca80 v29tx_control  WRITTEN (F10107)
 *   0x09ca90 v29rx_control  WRITTEN (F10107)
 *   0x09caa0 v29tx_message  already in class1tx.c
 *   0x09cad0 v29rx_message  already in class1tx.c
 *
 * ALL 40 non-message symbols are now written here.  The last three --
 * v17tx_control, v29tx_control, v29rx_control -- were blocked on their own
 * matching `V??_control` (`v17.c`/`v29.c`); `V17TX_control`/`V29TX_control`
 * landed together (F10107), which is also the pass that noticed
 * `V29RX_control` itself had already landed in wave 9 (F10103) while this
 * comment and the "BLOCKED" note beside `v29rx_control` in `faxadapt.c`
 * both still called it unwritten.  `init_vmi_v17tx`/`init_vmi_v27tx`/
 * `init_vmi_v29tx` (F9271's "check with nm -S" question, F10010) are NOT in
 * this file and never were candidates for it: their addresses (0x94870,
 * 0x94a70, 0x94970)
 * fall in the `class1tx.c` span (0x94870..0xac960), immediately AFTER the
 * `class1rx.c` span (0x93e80..0x94870) that holds their RX siblings
 * `init_vmi_v17rx`/`init_vmi_v27rx`/`init_vmi_v29rx` -- adjacent spans, not
 * the same one, and neither is this file's own range (0x9bf20..0x9cafd).
 * `readyqueue.py --span 'class1tx.c +94'` already attributes all three to
 * `class1tx.c`, which is owned this wave by the agent working `class1*.c`,
 * not by this file.
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

/* ---- create: every `*_create` in the 48-symbol TU is now written -------- */

void v17tx_create(struct faxvmi_link *dp, const struct v17tx_cfg *cfg);
void v17rx_create(struct faxvmi_link *dp, const struct v17rx_cfg *cfg);
void v21tx_create(struct faxvmi_link *dp, const struct v21tx_cfg *cfg);
void v21rx_create(struct faxvmi_link *dp, const struct v21rx_cfg *cfg);
void v27tx_create(struct faxvmi_link *dp, const struct v27tx_cfg *cfg);
void v27rx_create(struct faxvmi_link *dp, const struct v27rx_cfg *cfg);
void v29tx_create(struct faxvmi_link *dp, const struct v29tx_cfg *cfg);
void v29rx_create(struct faxvmi_link *dp, const struct v29rx_cfg *cfg);

/* ---- delete: `V??_delete(dp->int_0014)`, tail-called in the object ----- */

void v17tx_delete(struct faxvmi_link *dp);
void v17rx_delete(struct faxvmi_link *dp);
void v21tx_delete(struct faxvmi_link *dp);
void v21rx_delete(struct faxvmi_link *dp);
void v27tx_delete(struct faxvmi_link *dp);
void v27rx_delete(struct faxvmi_link *dp);
void v29tx_delete(struct faxvmi_link *dp);
void v29rx_delete(struct faxvmi_link *dp);

/* ---- status: `V??_status(dp->int_0014, status)`, tail-called ----------- */

int v17tx_status(struct faxvmi_link *dp, struct v17_status *status);
int v17rx_status(struct faxvmi_link *dp, struct v17_status *status);
int v21tx_status(struct faxvmi_link *dp, struct v21_status *status);
int v21rx_status(struct faxvmi_link *dp, struct v21_status *status);
int v27tx_status(struct faxvmi_link *dp, void *status);
int v27rx_status(struct faxvmi_link *dp, void *status);
int v29tx_status(struct faxvmi_link *dp, void *status);
int v29rx_status(struct faxvmi_link *dp, void *status);

/*
 * ---- process: drive one block through `V??_modem` and relay the count --
 *
 * THE TX AND RX SHAPES ARE MIRRORED, NOT THE SAME SHAPE TWICE.  Both read
 * `dp->int_0014` as the handle and both zero a caller `count` after copying
 * it to a caller `result`, but which formal is `in`/`out` and which pair is
 * (result, count) vs (count, result) differs by side -- read directly off
 * `dis.py`, not assumed:
 *
 *   TX: V??TX_modem(handle, dp->buf, out, count); *result = *count; *count=0;
 *   RX: V??RX_modem(handle, in, dp->buf, count);  *result = *count; *count=0;
 *
 * so TX's own second argument is `out` and RX's is `in`, and the count
 * pointer sits at formal index 2 for TX but index 3 for RX.  `v27tx_process`
 * is the one BLOCKED member (V27TX_modem unwritten) and is not declared.
 */

void v17tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		   unsigned short *result);
void v17rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		   unsigned short *count);
void v21tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		   unsigned short *result);
void v21rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		   unsigned short *count);
void v27tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		   unsigned short *result);
void v27rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		   unsigned short *count);
void v29tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		   unsigned short *result);
void v29rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		   unsigned short *count);

/*
 * ---- control: `V??_control(dp->int_0014, arg)`, tail-called ------------
 *
 * All twelve are now written -- `v17tx_control`/`v29tx_control`/
 * `v29rx_control` (F10107) were the last three, unblocked by
 * `V17TX_control`/`V29TX_control` landing and by re-checking `V29RX_control`
 * (wave 9, F10103) against the stale "BLOCKED" comment this header and
 * `faxadapt.c` both still carried for it.  `V27TX_control`/`V27RX_control`
 * take an untyped `void *`, not a struct pointer (v27fax.h).
 */

int v17tx_control(struct faxvmi_link *dp,
		  const struct v17tx_control_req *arg);
int v17rx_control(struct faxvmi_link *dp, const struct v17rx_ctl *arg);
int v21tx_control(struct faxvmi_link *dp, const struct v21tx_ctl *arg);
int v21rx_control(struct faxvmi_link *dp, const struct v21rx_ctl *arg);
int v27tx_control(struct faxvmi_link *dp, void *req);
int v27rx_control(struct faxvmi_link *dp, void *req);
int v29tx_control(struct faxvmi_link *dp,
		  const struct v29tx_control_req *arg);
int v29rx_control(struct faxvmi_link *dp,
		  const struct v29rx_control_req *arg);

#endif /* DSPLIB_FAXADAPT_H */
