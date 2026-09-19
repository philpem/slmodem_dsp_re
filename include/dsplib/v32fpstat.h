/*
 * v32fpstat.h -- ITU-T V.32 / V.32bis: the FP layer's four dispatch entry
 *                points and the status block one of them fills.
 *
 * `v32fpctl.h` is the twenty-one small setters the handshake calls; this is
 * the four functions the DATAPUMP GLUE calls, and they are a different
 * surface:
 *
 *     V32FP_create    .text 0x07f710   169   build the object from a config
 *     V32FP_modem     .text 0x082630   356   one block, through V32_PROTOCOL
 *     V32FP_control   .text 0x084530   776   apply a control request
 *     V32FP_status    .text 0x084840  1084   report, and post a request back
 *
 * `struct v32fp_ctl` -- what `V32FP_control` consumes -- is `v32fp.h`'s,
 * beside the `V32_CTL` template it is built from.  `struct v32_status` is
 * here because `V32FP_status` is the only thing that fills it.
 */

#ifndef DSPLIB_V32FPSTAT_H
#define DSPLIB_V32FPSTAT_H

#ifdef __cplusplus
extern "C" {
#endif

struct v32fp_ctl;
struct v32fp_params;
struct v32_modem;

/*
 * ---------------------------------------------------------------------------
 * The configuration `V32FP_create` is handed: 24 bytes.
 *
 * `v32_create` builds one on the stack and `V32FP_create` patches six fields
 * of the `V32_CFG` template from it.  Every field is named for WHERE IT LANDS
 * in `struct v32fp_params`, which is the only thing established about any of
 * them; the author's own letters for five of the six are in the format string
 * `v32: V32 config S%d,R%d,T%d,A%d,L%d` (.rodata.str1.4 + 0x72c), printed by
 * `v32_create` with, in order, +0x00, +0x14, +0x08, +0x10 and +0x0c.  Single
 * letters are not names, so they are recorded here and not used.
 */
struct v32fp_cfg {
	int protocol;		/* +0x00 S; -> params.protocol as (!= 0)     */
	/*
	 * +0x04.  `v32_create` computes it from `MDMPRM_IODELAY`, and
	 * `V32FP_create` puts its low half in `params.ec_near_delay` -- the
	 * echo canceller's near-end delay.
	 */
	int phys_delay;		/* +0x04                                     */
	int timeout;		/* +0x08 T; -> params.timeout          60000 */
	int energy_drop_time;	/* +0x0c L; -> params.energy_drop_time   700 */
	/*
	 * +0x10.  Only bit 0 is read, and it becomes bit 10 of
	 * `params.options` -- the single bit `V32FP_create` patches there.
	 * What that option IS is not established, so the field keeps its
	 * offset rather than taking the format string's bare `A`.
	 */
	int r10;		/* +0x10 A; 1 from v32_create                */
	/*
	 * +0x14 R, in bit/s.  It becomes BOTH `params.tx_rate` and
	 * `params.rx_rate`, and `params.trellis` is set from the same value by
	 * the comparison `rate > 0x1c1f` -- faster than 7200.
	 */
	unsigned short rate;	/* +0x14                                     */
	/*
	 * +0x16.  `v32_create` writes the same rate here and nothing
	 * reconstructed reads it.  Modelled, unnamed.
	 */
	unsigned short r16;	/* +0x16                                     */
};

/*
 * ---------------------------------------------------------------------------
 * The status block `V32FP_status` fills: 40 bytes.
 *
 * Field widths are all forced by the stores at 8497b..84a85.  Two of them are
 * named by the format string `v32: v32_update connect: tx_rate %d, rx_rate %d`
 * (.rodata.str1.4 + 0x7a4), which `v32_process` prints with +0x02 and +0x04 in
 * that order; the rest keep their offsets.
 */
struct v32_status {
	/*
	 * +0x00 <- PROTOCOL[hdx->mode], a nine-entry `.data` table that is
	 * file-local in the object and shares its name with a second, unrelated
	 * `PROTOCOL` in `.rodata`.  `symmap.py` therefore leaves both LOCAL and
	 * no `ref_PROTOCOL` exists, so this field is the only way a test can
	 * see that table at all.
	 */
	short protocol;		/* +0x00                                     */
	short tx_rate;		/* +0x02 <- RATEv32[fp + 0x2c]  bit/s        */
	short rx_rate;		/* +0x04 <- RATEv32[fp + 0x2e]  bit/s        */
	short r06;		/* +0x06 <- 1 - (fp + 0x256 >> 1)            */
	short snr;		/* +0x08 the SNR estimate; see below         */
	short r0a;		/* +0x0a                                     */
	short r0c;		/* +0x0c <- fp + 0x1f8                       */
	short r0e;		/* +0x0e always 0                            */
	short r10;		/* +0x10 <- fp + 0x6c                        */
	short r12;		/* +0x12 <- fp + 0x182                       */
	/*
	 * +0x14 is a bit set built one bit at a time with `andb`/`orb`, every
	 * one a READ-MODIFY-WRITE of the caller's byte.  BIT 6 IS FORCED TO
	 * ZERO by a statement of its own (84a5c/84a5f store `& 0xbf` and then
	 * `& 0x3f`), so nothing of the caller's survives.
	 *
	 *   bit 0  fp + 0x1c bit 0          bit 4  fp + 0x0c == 0
	 *   bit 1  fp + 0x20 bit 0          bit 5  fp + 0x10 == 0
	 *   bit 2  fp + 0x24 bit 0          bit 7  obj + 0x11 bit 1
	 *   bit 3  fp + 0x00 == 0
	 *
	 * Those are the same six `int` switches `V32FP_control` sets from
	 * `struct v32fp_ctl::ctl0`, read back with the same three inverted --
	 * so this half of the byte is `ctl0` reported, and `v32_data` and
	 * `V32FP_status` both copy it straight back into the request they
	 * build.  Not named for the same reason `ctl0`'s bits are not.
	 */
	unsigned char flags;	/* +0x14                                     */
	unsigned char flags1;	/* +0x15 bit 0 <- obj + 0x11 bit 2           */
	short r16;		/* +0x16 not written                         */
	int r18;		/* +0x18 <- fp + 0x188                       */
	int r1c;		/* +0x1c <- (fp + 0x1f4 != 1)                */
	int r20;		/* +0x20 always 0                            */
	int r24;		/* +0x24 always 0                            */
};

/* ------------------------------------------------------------------------ */

/**
 * @brief Build or rebuild a V.32 datapump instance.
 *
 * @p modem null means "allocate one"; a non-null one is reconfigured in
 * place. @p params null means "use `V32_CFG`". Its first 48 bytes are
 * copied over the object's, which is why `V32FP_control` can and does pass
 * the object as both arguments. @p arg2 is forwarded from `V32FP_create`'s
 * own second argument and is 0 at every written call site.
 *
 * @param modem   NULL to allocate, or an existing instance to reconfigure.
 * @param params  The 48-byte parameter block to copy in, or NULL for `V32_CFG`.
 * @param arg2    Forwarded from `V32FP_create`'s second argument (always 0 in practice).
 * @return The instance, allocated or not.
 */
void *V32FP_recreate(struct v32_modem *modem,
		     const struct v32fp_params *params, void *arg2);

/**
 * @brief Build a V.32 datapump instance from the caller's compact configuration.
 *
 * Copies `V32_CFG` to the stack, patches six fields from @p cfg, and hands
 * the result to V32FP_recreate() with a null object, which is what makes
 * V32FP_recreate() allocate one.
 *
 * @param cfg   The caller's configuration.
 * @param arg1  Forwarded to V32FP_recreate()'s third argument.
 * @return The new instance.
 */
void *V32FP_create(const struct v32fp_cfg *cfg, void *arg1);

/**
 * @brief Run one block of V.32 modulation and demodulation.
 *
 * @p nout is in/out -- transmit bits in, output samples out -- and @p nin
 * is in/out the other way round, input samples in and receive bits out.
 * Dispatches through `V32_PROTOCOL[hdx->mode]` and scales the output by
 * `params.tx_scale`.
 *
 * @param modem   The V.32 datapump instance.
 * @param txbits  Bits to transmit.
 * @param out     Output for the modulated transmit samples.
 * @param in      Received samples to demodulate.
 * @param rxbits  Output for the demodulated bits.
 * @param nout    In/out: transmit bit count, then output sample count.
 * @param nin     In/out: input sample count, then received bit count.
 * @return `V32_OBJ_STATUS`.
 */
int V32FP_modem(struct v32_modem *modem, const int *txbits, short *out, const short *in,
		int *rxbits, int *nout, int *nin);

/**
 * @brief Apply a V.32 control request.
 * @param modem  The V.32 datapump instance.
 * @param ctl    The control request.
 * @return Always 1.
 */
int V32FP_control(struct v32_modem *modem, struct v32fp_ctl *ctl);

/**
 * @brief Fill a V.32 status report, and post a control request of its own.
 * @param modem  The V.32 datapump instance.
 * @param st     Output: the status report.
 * @return Always 1.
 */
int V32FP_status(struct v32_modem *modem, struct v32_status *st);

/**
 * @brief `V32_PROTOCOL`'s data-mode handler: one block of steady-state V.32 traffic.
 *
 * File-local in the object, so it is `static` in v32fpdisp.c and declares
 * nothing here; the differential test declares it the ordinary way (its
 * address is taken by `V32_PROTOCOL`, so the convention does not change)
 * and reaches it through the globalized test copy (tools/testvisible.py).
 * Its argument list is `v32_handshake`'s, because one table dispatches to
 * both.
 *
 * @param modem     The V.32 datapump instance.
 * @param txdata    Transmit symbols.
 * @param txout     Output for the modulated transmit samples.
 * @param rxin      Received samples to demodulate.
 * @param rxout     Output for the demodulated receive symbols.
 * @param nsamples  Sample count.
 * @param rxcount   Receive count.
 */

/*
 * `V32_PROTOCOL`: nine function pointers indexed by `V32HDX_MODE`, GLOBAL in
 * the object at `.data:0x007700`.  The element type is `v32_handshake`'s own
 * signature, which `v32hdx.h` derived from its two callees; `v32_null_protocol`
 * occupies three of the nine slots and is `void (void)`, so those three are
 * cast, exactly as `v23_ops` casts `dp_wrapper_run`.
 *
 * The nine-entry `short` table beside it -- `PROTOCOL`, `.data:0x007768`, which
 * `V32FP_status` reports through -- is NOT declared here and is `static` in
 * `v32fpdisp.c`.  It is file-local in the object AND its name is used by a
 * second translation unit, so `symmap.py` leaves both copies local and there
 * is no `ref_PROTOCOL` for a test to name; making ours global would export a
 * name as generic as `PROTOCOL` from the library and buy nothing.  It is
 * reached through `struct v32_status::protocol` instead.
 */
typedef void (*v32_protocol_fn)(struct v32_modem *modem, unsigned short *txdata,
				short *txout, short *rxin,
				unsigned short *rxout, short *nsamples,
				unsigned short *rxcount);

extern v32_protocol_fn V32_PROTOCOL[9];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32FPSTAT_H */
