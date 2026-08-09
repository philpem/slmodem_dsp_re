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
extern long modem_get_param(void *modem, unsigned param);
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
 * it is.  The true size is unknown and is at least 0x7c.
 *
 * THE FIELD NAMES ARE DESCRIPTIONS OF USE, NOT RECOVERED NAMES.  Finding 226:
 * the mangling preserves the type name and never a data member's.  What IS
 * measured is the offset, the width and the signedness of every access:
 *
 *   +0x000  `movzbl (%esi),%edx; and $0x1,%dl` in `V90Parameters::
 *           setToDefault` at .text+0x2a536 -- one byte, bit 0 only.
 *   +0x038  `mull 0x38(%ebx)` at +0x29971, against 0x1b4e81b5 with the
 *   +0x03c  product's high half shifted right 8: the exact unsigned
 *           magic-number division by 2400, so both are `unsigned int` bit
 *           rates and the quotient is a rate index.  Then `jae`/`jbe`
 *           throughout, which is unsigned again.
 *   +0x040  `mul $0xcccccccd; shr $2` in `loadModemParamsData` at +0x2a718 --
 *           unsigned division by 10, so `unsigned int`.  Printed `%d`.
 *   +0x048  compared against and copied into `LINE_CONNECTION_TYPE`, which
 *           the object initialises to -1 and tests for -1: signed.
 *   +0x050  `movzbl 0x50(%ebx)` twice in `loadModemParamsData`, bit 1 at
 *           +0x2a7bb and bit 0 at +0x2a7fa -- one byte.
 *   +0x078  loaded and, when non-zero, passed as `loadParams(char *)`'s only
 *           argument -- a parameter-file name.
 */
struct _tagModemParameters {
	unsigned char	sessionFlags;		/* +0x000 */
	unsigned char	unmapped_0001[0x38 - 0x01];
	unsigned int	minRate;		/* +0x038 */
	unsigned int	maxRate;		/* +0x03c */
	unsigned int	powerReductionTenths;	/* +0x040 */
	unsigned char	unmapped_0044[0x48 - 0x44];
	int		connectionType;		/* +0x048 */
	unsigned char	unmapped_004c[0x50 - 0x4c];
	unsigned char	modeFlags;		/* +0x050 */
	unsigned char	unmapped_0051[0x78 - 0x51];
	char		*paramFile;		/* +0x078 */
};

#endif /* DSPLIB_MODEM_PARAMS_H */
