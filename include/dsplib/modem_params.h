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

#endif /* DSPLIB_MODEM_PARAMS_H */
