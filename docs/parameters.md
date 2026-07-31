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

## UNITS: the cadence times are in tens of milliseconds

**Every `*CadenceOnTime` and `*CadenceOffTime` in the country table is a count
of 10 ms units, not milliseconds and not samples.** A 500 ms busy tone is 50.

Nothing in the object says so; it falls out of the conversion
`cadence_create` applies to each of the four windows before storing it:

```
    intervals = (GetFP_Value(1, buflen) * time * 80) >> 14
```

`GetFP_Value(1, b)` is `ceil(16384 / b)`, so the expression is
`time * 80 / buflen`, where `buflen` is the number of samples between
`toneiir` verdicts -- the unit `cadence_progress` counts in.

`buflen` is **per tone**, and only the dial-tone detector takes it from the
country table:

```
    DIAL              GetCallProgressSamplesBufferLength, or 666 if that is 0
    BUSY, CONG, RING  160, hard-coded
```

For the result to be a count of those intervals:

```
    intervals = time_seconds * 8000 / buflen
             => time * 80 / buflen = time_seconds * 8000 / buflen
             => time_seconds = time / 100
             => time is in units of 10 ms
```

Note the conclusion does not depend on `buflen` -- it cancels -- which is the
point of writing the conversion that way.

Worked through for busy tone, where `buflen` is 160 and an interval is 20 ms:
a 500 ms busy tone is `50` in the table, and `50 * 80 / 160 = 25` intervals,
which is 500 ms. And for dial tone, where `buflen` is 666 and an interval is
83.25 ms: a 500 ms validation time is `50`, and `50 * 80 / 666 = 6` intervals,
which is 500 ms. Both close.

This matters beyond cadence, because the same 10 ms convention almost
certainly applies to the other durations in `struct homolog_params` --
`HookFlashTime`, `DialPauseTime`, `DialToneValidationTime`,
`PulseBetweenDigitsInterval`. Those are read by the dialler and by
`CALLPROG_Create`, which are not reconstructed yet, so treat the extension as
a strong expectation rather than an established fact until each one is
checked against its own arithmetic.

The two pulse-dial times are the known exception in the other direction:
`GetPulseDialMakeTime` and `GetPulseDialBreakTime` go to `SetPulseMakeTime`
and `SetPulseBreakTime`, whose own conversion has not been read yet.

## The other namespace: S-registers

`CALLPROG_Dial` fetches the calling tone's level through an indirect callback
stored in the CALLPROG object at +0x20, with the index `0xdd` -- 221, far
outside the enum above. **It is not a parameter index. It is an AT
S-register.**

The callback is installed by `CALLPROG_Create` from its configuration block,
and `call_create` supplies `call_GetSRegister`, which is fourteen bytes:

```
    2b60:  movzwl 0x8(%esp),%eax        ; narrow the index to 16 bits
    2b65:  mov    %eax,0x8(%esp)
    2b69:  jmp    modem_get_sreg        ; and tail-call
```

slmodemd's `modem_get_sreg` indexes `unsigned char sregs[256]` and returns -1
above that, so 221 is comfortably in range and answers with `sregs[221]`.

So the calling tone's transmit level is **S221**, settable from the AT command
line like any other S-register, and the library reads it through a callback
precisely so that call.c can decide where S-registers come from.

This resolves a question carried since finding 44. It is *not* a defect: the
index is valid, the namespace is real, and nothing is out of range. D13 -- the
level control that spans 1.4 dB across its whole argument -- stands on its own
and is unaffected.

One detail follows from it. `modem_get_sreg` returns an `unsigned char`
promoted to `long`, and `CALLPROG_Dial` narrows it with `movsbl` before
passing it on, so an S221 of 128 or more arrives at `ResetCallingTone` as a
negative level. Given D13, that changes the amplitude by well under a
decibel.
