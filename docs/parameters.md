# Modem parameters

`modem_get_param(modem, n)` is how the library asks its host for
configuration. The indices are not guessable from the object -- they appear
only as immediates -- but slmodemd still carries the enum they come from, in
`modem_param.h`, and most of the entries are named after exactly what the
call-progress and dialler code does with them.

## Reading the enum correctly

The list contains one alias, `MDMPRM_RATE = MDMPRM_RX_RATE`, and it is easy to
mis-index: the alias does not consume a value, but the enumerator *after* it
continues from the alias's value plus one. Getting that wrong shifts
everything from `MDMPRM_TX_RATE` onward down by one and looks entirely
plausible.

Four independent anchors in the object confirm the table below:

| use in the object | index | name |
| --- | --- | --- |
| `dp_param_get`, already tested in phase 1 | 10 | `MDMPRM_DPRUNTIME` |
| `call_create` fetches the dial string and checks its first character is a digit | 7 | `MDMPRM_DIALSTR` |
| `call_create` passes it to `SetPulseMakeTime` | 17 | `GetPulseDialMakeTime` |
| `CALLPROG_Dial` gates the calling tone on it being 2 | 27 | `GetCallingToneFlag` |

## The table

| index | hex | name |
| --- | --- | --- |
| 0 | 0x00 | `MDMPRM_NONE` |
| 1 | 0x01 | `MDMPRM_RX_RATE` |
| 1 | 0x01 | `MDMPRM_RATE` *(alias)* |
| 2 | 0x02 | `MDMPRM_TX_RATE` |
| 3 | 0x03 | `MDMPRM_MIN_RATE` |
| 4 | 0x04 | `MDMPRM_MAX_RATE` |
| 5 | 0x05 | `MDMPRM_IODELAY` |
| 6 | 0x06 | `MDMPRM_CODECTYPE` |
| 7 | 0x07 | `MDMPRM_DIALSTR` |
| 8 | 0x08 | `MDMPRM_AUTOMODE` |
| 9 | 0x09 | `MDMPRM_DP_REQUESTED` |
| 10 | 0x0a | `MDMPRM_DPRUNTIME` |
| 11 | 0x0b | `MDMPRM_DSPINFO` |
| 12 | 0x0c | `MDMPRM_VOICEINFO` |
| 13 | 0x0d | `MDMPRM_UPDATE_DELAY` |
| 14 | 0x0e | `MDMPRM_DP_ADDR` |
| 15 | 0x0f | `MDMPRM_HOOK_ON` |
| 16 | 0x10 | `MDMPRM_PULSE_DIAL` |
| 17 | 0x11 | `GetPulseDialMakeTime` |
| 18 | 0x12 | `GetPulseDialBreakTime` |
| 19 | 0x13 | `GetPulseDialDigitPattern` |
| 20 | 0x14 | `GetDTMFHighToneLevel` |
| 21 | 0x15 | `GetDTMFDialSpeed` |
| 22 | 0x16 | `GetMinBusyCadenceOnTime` |
| 23 | 0x17 | `GetMaxBusyCadenceOnTime` |
| 24 | 0x18 | `GetBusyDetectionCyclesNumber` |
| 25 | 0x19 | `GetMinBusyCadenceOffTime` |
| 26 | 0x1a | `GetMaxBusyCadenceOffTime` |
| 27 | 0x1b | `GetCallingToneFlag` |
| 28 | 0x1c | `GetHookFlashTime` |
| 29 | 0x1d | `GetBlindDialPause` |
| 30 | 0x1e | `GetNoAnswerTimeOut` |
| 31 | 0x1f | `GetDialPauseTime` |
| 32 | 0x20 | `GetTransmitLevel` |
| 33 | 0x21 | `GetDialModifierValidation` |
| 34 | 0x22 | `GetDialToneValidationTime` |
| 35 | 0x23 | `GetBusyToneDiffTime` |
| 36 | 0x24 | `GetDTMFHighAndLowToneLevelDifference` |
| 37 | 0x25 | `GetPulseDialingFlag` |
| 38 | 0x26 | `GetDialToneCallProgressFilterIndex` |
| 39 | 0x27 | `GetDialToneDetectionThreshold` |
| 40 | 0x28 | `GetABCDDialingPermittedFlag` |
| 41 | 0x29 | `GetComaPauseDurationLimit` |
| 42 | 0x2a | `GetPulseAndToneDialInSameDialStringPermittedFlag` |
| 43 | 0x2b | `GetBusyToneCallProgressFilterIndex` |
| 44 | 0x2c | `GetPulseBetweenDigitsInterval` |
| 45 | 0x2d | `GetDialToneWaitTime` |
| 46 | 0x2e | `GetMinRingbackCadenceOnTime` |
| 47 | 0x2f | `GetMaxRingbackCadenceOnTime` |
| 48 | 0x30 | `GetRingbackDetectionCyclesNumber` |
| 49 | 0x31 | `GetMinRingbackCadenceOffTime` |
| 50 | 0x32 | `GetMaxRingbackCadenceOffTime` |
| 51 | 0x33 | `GetRingbackToneCallProgressFilterIndex` |
| 52 | 0x34 | `GetMinCongestionCadenceOnTime` |
| 53 | 0x35 | `GetMaxCongestionCadenceOnTime` |
| 54 | 0x36 | `GetCongestionDetectionCyclesNumber` |
| 55 | 0x37 | `GetMinCongestionCadenceOffTime` |
| 56 | 0x38 | `GetMaxCongestionCadenceOffTime` |
| 57 | 0x39 | `GetCongestionToneCallProgressFilterIndex` |
| 58 | 0x3a | `GetCallProgressSamplesBufferLength` |
| 59 | 0x3b | `MustNoiseFilterBeApplied` |
| 60 | 0x3c | `GetAdditAttenToBeepgenVoice` |
| 61 | 0x3d | `GetDialToneFilterSubindex` |
| 62 | 0x3e | `GetBusyToneLooseDetectionEnabled` |
| 63 | 0x3f | `MDMPRM_LAST` |
## The country table

Sixteen of these are per-country homologation settings, and slmodemd carries
the struct they live in as `struct homolog_params` in `modem_homolog.h` --
`PulseDialMakeTime`, `MinBusyCadenceOnTime`, `CallingToneFlag`,
`TransmitLevel` and the rest, one `u8` each, per country. That is what
`cadence_create` is reading when it asks for `GetMinBusyCadenceOnTime` and
friends: the busy-tone cadence a British modem should expect is not the one an
American modem should.

## The other namespace

`CALLPROG_Dial` also fetches through an indirect callback stored in the
CALLPROG object at +0x20, with the index `0xdd` (221) -- far outside this
enum. That is a second parameter namespace, and which one is not yet
established. It will be settled by `CALLPROG_Create`, which is what installs
the callback. The one use so far is the calling tone's level, and it does not
matter much what comes back: see D13.
