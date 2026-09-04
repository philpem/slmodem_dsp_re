/*
 * modem_params.h -- indices for modem_get_param / modem_set_param.
 *
 * The library asks its host for configuration by number.  The numbers appear
 * in the object only as immediates, but slmodemd still carries the enum they
 * come from (`enum MODEM_PARAMETER_NAMES` in modem_param.h), and most entries
 * are named after exactly what the call-progress and dialler code does with
 * them.  Reproduced here as macros, since this tree does not include
 * slmodemd's headers -- the values are fixed by an ABI that modem.o is
 * already compiled against.
 *
 * Sixteen of these are per-country homologation settings and live in
 * `struct homolog_params`: the busy cadence a British modem should expect is
 * not the one an American modem should.
 *
 * Watch the alias.  `MDMPRM_RATE = MDMPRM_RX_RATE` does not consume a value,
 * but the enumerator after it continues from that value plus one.  Getting it
 * wrong shifts everything from MDMPRM_TX_RATE down by one and looks entirely
 * plausible; four uses in the object pin the numbering.  See
 * docs/parameters.md.
 */

#ifndef DSPLIB_MODEM_PARAMS_H
#define DSPLIB_MODEM_PARAMS_H

#define MDMPRM_NONE                                      0
#define MDMPRM_RX_RATE                                   1
#define MDMPRM_RATE                                      1
#define MDMPRM_TX_RATE                                   2
#define MDMPRM_MIN_RATE                                  3
#define MDMPRM_MAX_RATE                                  4
#define MDMPRM_IODELAY                                   5
#define MDMPRM_CODECTYPE                                 6
#define MDMPRM_DIALSTR                                   7
#define MDMPRM_AUTOMODE                                  8
#define MDMPRM_DP_REQUESTED                              9
#define MDMPRM_DPRUNTIME                                 10
#define MDMPRM_DSPINFO                                   11
#define MDMPRM_VOICEINFO                                 12
#define MDMPRM_UPDATE_DELAY                              13
#define MDMPRM_DP_ADDR                                   14
#define MDMPRM_HOOK_ON                                   15
#define MDMPRM_PULSE_DIAL                                16
#define GetPulseDialMakeTime                             17
#define GetPulseDialBreakTime                            18
#define GetPulseDialDigitPattern                         19
#define GetDTMFHighToneLevel                             20
#define GetDTMFDialSpeed                                 21
#define GetMinBusyCadenceOnTime                          22
#define GetMaxBusyCadenceOnTime                          23
#define GetBusyDetectionCyclesNumber                     24
#define GetMinBusyCadenceOffTime                         25
#define GetMaxBusyCadenceOffTime                         26
#define GetCallingToneFlag                               27
#define GetHookFlashTime                                 28
#define GetBlindDialPause                                29
#define GetNoAnswerTimeOut                               30
#define GetDialPauseTime                                 31
#define GetTransmitLevel                                 32
#define GetDialModifierValidation                        33
#define GetDialToneValidationTime                        34
#define GetBusyToneDiffTime                              35
#define GetDTMFHighAndLowToneLevelDifference             36
#define GetPulseDialingFlag                              37
#define GetDialToneCallProgressFilterIndex               38
#define GetDialToneDetectionThreshold                    39
#define GetABCDDialingPermittedFlag                      40
#define GetComaPauseDurationLimit                        41
#define GetPulseAndToneDialInSameDialStringPermittedFlag 42
#define GetBusyToneCallProgressFilterIndex               43
#define GetPulseBetweenDigitsInterval                    44
#define GetDialToneWaitTime                              45
#define GetMinRingbackCadenceOnTime                      46
#define GetMaxRingbackCadenceOnTime                      47
#define GetRingbackDetectionCyclesNumber                 48
#define GetMinRingbackCadenceOffTime                     49
#define GetMaxRingbackCadenceOffTime                     50
#define GetRingbackToneCallProgressFilterIndex           51
#define GetMinCongestionCadenceOnTime                    52
#define GetMaxCongestionCadenceOnTime                    53
#define GetCongestionDetectionCyclesNumber               54
#define GetMinCongestionCadenceOffTime                   55
#define GetMaxCongestionCadenceOffTime                   56
#define GetCongestionToneCallProgressFilterIndex         57
#define GetCallProgressSamplesBufferLength               58
#define MustNoiseFilterBeApplied                         59
#define GetAdditAttenToBeepgenVoice                      60
#define GetDialToneFilterSubindex                        61
#define GetBusyToneLooseDetectionEnabled                 62
#define MDMPRM_LAST                                      63
/*
 * The store itself, declared the way slmodemd declares it.  The `long` is not
 * cosmetic: MDMPRM_DP_ADDR carries the datapump's address through this
 * otherwise int-shaped API, so a narrower return type truncates the pointer
 * anywhere `long` is wider than one.  `modem` is slmodemd's `struct modem *`,
 * opaque here.
 */

/**
 * @brief Read one host configuration/state value by parameter index.
 *
 * Implemented by the host (slmodemd), not by this library -- declared here
 * only so this tree's callers can link and be tested against a fake.
 *
 * @param modem  Host's opaque `struct modem *`.
 * @param param  One of the `MDMPRM_*` / `Get*` indices above.
 * @return The parameter's value. `long`-width because MDMPRM_DP_ADDR
 *         returns a pointer through this otherwise int-shaped API.
 */
extern long modem_get_param(void *modem, unsigned param);

/**
 * @brief Set one host configuration value by parameter index.
 *
 * Implemented by the host (slmodemd), not by this library.
 *
 * @param modem  Host's opaque `struct modem *`.
 * @param param  One of the `MDMPRM_*` / `Get*` indices above.
 * @param value  New value.
 * @return Host-defined status.
 */
extern long modem_set_param(void *modem, unsigned param, int value);

/*
 * `struct _tagModemParameters` -- the block V.PCM is handed at construction.
 *
 * NOT slmodemd's `struct modem`, and not the numbering above: this is a
 * separate, library-internal record whose address every `V90Parameters` and
 * `V92Parameters` keeps at its own +0x000.  The mangling names the type --
 * `_ZN13V90ParametersC1EP19_tagModemParameters` -- and nothing else in the
 * object names anything inside it, so the fields below are those the members
 * reconstructed so far actually touch, and nothing is claimed about the rest.
 *
 * THIS IS A PARTIAL LAYOUT AND IT IS MEANT TO BE EXTENDED, NOT REPLACED.
 * `V90Modem`, `V92Modem`, `VPcmFloModem` and `K56FlexFloModem` all carry one
 * of these and will read fields this does not name yet; the right move is to
 * turn a slice of `unmapped_*` into fields, keeping every offset below where
 * it is.
 *
 * IT IS ALSO slmodemd's `m->dp_runtime`, WHICH IS WHERE IT COMES FROM AND
 * WHAT SETS ITS SIZE.  That identification is three independent steps and no
 * step is a guess:
 *
 *   - `vpcm_create` stores `dp_param_get(modem)` at its root +0x28 (0x3ac2),
 *     and `dp_param_get` is `modem_get_param(modem, MDMPRM_DPRUNTIME)`
 *     (0x58c0, three instructions, already reconstructed in
 *     `src/core/dp_param.c`).  slmodemd answers that index with
 *     `m->dp_runtime` (`slmodemd/modem_param.c:79`).
 *   - `m->dp_runtime` is `dp_runtime_create(m)`'s return
 *     (`slmodemd/modem.c:1136`), and `dp_runtime_create` is in this object at
 *     0x58e0, immediately after `dp_param_get` and in the same translation
 *     unit.  It `sysdep_malloc`s **0x88** and initialises the fields below.
 *   - `vpcm_create` passes root +0x28 as the third argument of
 *     `VPCMXF_Create`, which passes it straight to
 *     `_ZN12VPcmFloModemC1EPv12V90ModemSideP19_tagModemParametersj20V90Comput
 *     ationalMode20V92ComputationalMode` at 0xfdcd.  The mangling types that
 *     argument `_tagModemParameters *`.
 *
 * So the size is **0x88 = 136**, by wave 0's own argument (the `sysdep_malloc`
 * immediately before the initialiser), and the writer of every field is
 * `dp_runtime_create` or `vpcm_create`.  Findings F820-823.
 *
 * THE FIELD NAMES ARE DESCRIPTIONS OF USE, NOT RECOVERED NAMES.  Finding F226:
 * the mangling preserves the type name and never a data member's.  What IS
 * measured is the offset, the width and the signedness of every access.  Four
 * names below are stronger than that and say so where they sit: two come from
 * the object's own `dsplibs_debug_printf` format strings, one from the
 * `modem_get_param` index whose answer it stores, and one from the parameter
 * whose value `vpcm_delete` copies back out.  A field this object only ever
 * writes a constant into is `unnamed_*`, with the constant recorded -- the
 * convention `V90Parameters.h` set for the same situation (finding F878).
 *
 *   +0x000  `movzbl (%esi),%edx; and $0x1,%dl` in `V90Parameters::
 *           setToDefault` at .text+0x2a536 -- one byte, bit 0 only.
 *   +0x002  four separate bit assignments in `dp_runtime_create` -- bit 4 set
 *           and bit 5 cleared at 0x5947-0x5952, bit 6 taken from
 *           `dsp_info.qc_lapm & 1` at 0x5955, bit 7 cleared at 0x5972.  Then
 *           `vpcm_create` OVERWRITES bit 4 with (session type == V.92) at
 *           0x3ba4-0x3bb4 and clears bit 5 again at 0x3bbf, and reads bit 4
 *           back at 0x3bca to compute root +0xd250.  One byte throughout.
 *   +0x010  `dsp_info.qc_index` if non-zero, otherwise the literal 9
 *           (0x5963-0x5975 with the `mov $0x9` arm at 0x5a00).
 *   +0x030  the pair `vpcm: VPCM rate limits: %d-%d\n` prints, straight from
 *   +0x034  `modem_get_param`'s MDMPRM_MIN_RATE and MDMPRM_MAX_RATE, the
 *           second clamped to 0xdac0 = 56000 (0x3b39-0x3b8d).  NOT the pair
 *           below: these two are the host's window and are never divided.
 *   +0x038  `mull 0x38(%ebx)` at +0x29971, against 0x1b4e81b5 with the
 *   +0x03c  product's high half shifted right 8: the exact unsigned
 *           magic-number division by 2400, so both are `unsigned int` bit
 *           rates and the quotient is a rate index.  Then `jae`/`jbe`
 *           throughout, which is unsigned again.  `vpcm_create` writes them
 *           as the LITERALS 0x12c0 = 4800 and 0x8340 = 33600 (0x3b90), on
 *           both arms, whatever the host asked for.
 *   +0x040  `mul $0xcccccccd; shr $2` in `loadModemParamsData` at +0x2a718 --
 *           unsigned division by 10, so `unsigned int`.  Printed `%d`.
 *   +0x048  compared against and copied into `LINE_CONNECTION_TYPE`, which
 *           the object initialises to -1 and tests for -1: signed.  It is
 *           `dsp_info` +0x00 on the way in (0x59ad) and on the way back out
 *           (`vpcm_delete` 0x3df0-0x3df3), so it PERSISTS ACROSS A DATAPUMP
 *           CHANGE -- that round trip is the whole point of the field.
 *   +0x04c  `dsp_info` +0x04 in (0x5980) and out (0x3dea-0x3ded), the same
 *           round trip.  Declared `int` and not `long`: this tree also builds
 *           64-bit and the object's width is four bytes.
 *   +0x050  `movzbl 0x50(%ebx)` twice in `loadModemParamsData`, bit 1 at
 *           +0x2a7bb and bit 0 at +0x2a7fa -- one byte.
 *   +0x054  `modem_get_param(modem, MDMPRM_CODECTYPE)` at 0x59c0.
 *   +0x064  the pair `vpcm: Delays: HW %d, DMA %d\n` prints (0x3d00-0x3d15).
 *   +0x068  HW is MDMPRM_IODELAY + 4; DMA is HW - 0x30 plus root +0xd254,
 *           and 0x30 = 48 is slmodemd's own `ST7554_HW_IODELAY (48)`
 *           (`slmodemd/modem_main.c:682`).  Both are `%d`, so signed -- DMA
 *           is -44 for an iodelay of 0 and the object prints it that way.
 *   +0x06c  the third `%d` of the SAME two format strings that name the pair
 *           above: `vpcm: P2 FINISHED: increase delay!! init %d, ext %d,
 *           add %d` at 0x4200 and `vpcm: P2 RESTART: decrease delay!!` at
 *           0x4393, where init is +0x064 and ext is +0x068.  `vpcm_run`'s
 *           phase-II arms are the only writers: arm 1 stores root +0xd254
 *           into it and asks the host for that much more delay, arm 0 stores
 *           zero and gives it back, and each is gated on the other having
 *           happened -- so it is "how much extra delay is currently taken",
 *           and `> 0` (0x432b, signed) is the test.  `vpcm_create` and
 *           `dp_runtime_create` only ever zero it, which is why it was
 *           `unnamed_006c` until finding F983.
 *   +0x078  loaded and, when non-zero, passed as `loadParams(char *)`'s only
 *           argument -- a parameter-file name.  `vpcm_create` NULLs it at
 *           0x3ad2, which is the store that made a bogus DPRUNTIME fault.
 */
struct _tagModemParameters {
	unsigned char	sessionFlags;		/* +0x000 */
	unsigned char	unmapped_0001[0x02 - 0x01];
	unsigned char	qcFlags;		/* +0x002 */
	unsigned char	unnamed_0003;		/* +0x003 */  /* low 3 bits cleared */
	int		unnamed_0004;		/* +0x004 */  /* = 60 */
	int		unnamed_0008;		/* +0x008 */  /* = 40 */
	int		unnamed_000c;		/* +0x00c */  /* = 0 */
	int		qcIndex;		/* +0x010 */
	int		unnamed_0014;		/* +0x014 */  /* = 700 */
	unsigned char	unmapped_0018[0x30 - 0x18];
	unsigned int	vpcmRateLimitLow;	/* +0x030 */
	unsigned int	vpcmRateLimitHigh;	/* +0x034 */
	unsigned int	minRate;		/* +0x038 */
	unsigned int	maxRate;		/* +0x03c */
	unsigned int	powerReductionTenths;	/* +0x040 */
	int		unnamed_0044;		/* +0x044 */  /* = 6 */
	int		connectionType;		/* +0x048 */
	int		clockDeviation;		/* +0x04c */
	unsigned char	modeFlags;		/* +0x050 */
	unsigned char	unmapped_0051[0x54 - 0x51];
	int		codecType;		/* +0x054 */
	int		unnamed_0058;		/* +0x058 */  /* = 0 */
	int		unnamed_005c;		/* +0x05c */  /* = 0 */
	int		unnamed_0060;		/* +0x060 */  /* = 0 */
	int		hwDelay;		/* +0x064 */
	int		dmaDelay;		/* +0x068 */
	int		addedDelay;		/* +0x06c */
	unsigned char	unmapped_0070[0x78 - 0x70];
	char		*paramFile;		/* +0x078 */
	unsigned char	unmapped_007c[0x88 - 0x7c];
};

/*
 * `struct dsp_info` -- the host's per-line record, MDMPRM_DSPINFO's answer.
 *
 * `vpcm_create` stores `modem_get_param(modem, MDMPRM_DSPINFO)` at its root
 * +0x24 (0x3abf) and never dereferences it; `vpcm_delete` writes two words
 * through it (0x3de7-0x3df3) and `dp_runtime_create` reads four (0x5955,
 * 0x5963, 0x5980, 0x59ad).  Those four accesses are at +0x00, +0x04, +0x08
 * and +0x0c, all four bytes wide, and they are the whole structure --
 * slmodemd declares exactly those four members in that order
 * (`slmodemd/modem_defs.h:366`), so the NAMES are the host's and the OFFSETS
 * and WIDTHS are this object's.  `long clock_deviation` there is `int` here
 * for the reason given above.
 *
 * It is 16 bytes and it is the host's storage, not the library's: nothing in
 * this object allocates one.  A test that constructs V.PCM must own one.
 */
struct dsp_info {
	unsigned int	connection_type;	/* +0x000 */
	int		clock_deviation;	/* +0x004 */
	unsigned int	qc_lapm;		/* +0x008 */
	unsigned int	qc_index;		/* +0x00c */
};

#endif /* DSPLIB_MODEM_PARAMS_H */
