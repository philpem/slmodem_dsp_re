# Reject silent input in phase-2 INFO0 repeat detection

The original Smart Link DSP can mistake silence for a repeated INFO0 message
and restart V.34 phase 2 just before transmitting its line probe. This is the
defect already described in [D77](deviations.md).
The reconstructed normal build now qualifies that detection by signal level.
`DSPLIB_REPRODUCE_BUGS` preserves the original behaviour for differential tests.
The bug report is [issue #294](https://github.com/philpem/slmodem_dsp_re/issues/294).

## Trigger and provenance

The shared `TX_PHASE2_ANS` / `TX_PHASE2_CALL` arm accepts
`(fsk.sr & 15) == 15` while the recovery field is zero. The original object
contains this test at `0x6683b` and its second guard at `0x6cc1b`, with no
signal-level qualification. The real FSK demodulator continues to produce
ones from silent input. This is an inherited Smart Link defect; the
reconstruction faithfully reproduced it.

A separate failing Olinet-originated call, recorded with reconstructed-only
handshake observation, supplied `[1, 0, -2, -3]` at the demodulator input,
after the AGC, with 1.87 counts RMS. Its shift register changed from `0x0207` to
`0x040f` and its bit count from 236 to 237. The repeat detector then reset
phase 2 at counter 85, before the normal transition at 96. The preceding
Tone B measured approximately 3,348 counts RMS. These observations establish
a fresh sliced bit, rather than an initialiser's all-one fill.

ITU-T V.34 (02/98), section 11.2.1.1.3, requires the caller to transmit
silence after its Tone B phase reversal and the following 10 ms of Tone B.
Silence during this part of the handshake is expected.

The canonical reference object has SHA-256
`1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379`.
The deployed D-Modem original control has SHA-256
`1129d826ca23f1e33aeb0eb8211ef0f91788e82549941a143e3181025d4fa10c`.
Both contain the same unqualified guard at these addresses. The original
control object was left unchanged.

## Correction and regression

The corrected arm requires a four-sample mean-square level of at least
`64 * 64` at the AGC output before accepting the existing four-one prefix.
Each square is divided by four before accumulation, avoiding overflow even
for four full-scale negative samples. The floor is about 34 dB below the
preceding tone in the measured Olinet call. It rejects the observed quiet
interval while retaining detection at normal handshake levels.

This is a signal-level guard, not a complete INFO0 frame validator. It does
not establish rejection of every noise waveform, and broader noise-floor or
attenuation coverage remains useful.

`test/unit/t_v34info0carrier.c` uses the existing handshake arena. It passes
silence through the real demodulator until at least four fresh bits appear,
then feeds the receive queue through the real AGC and handshake dispatch.
Its 24 seeded component cases cover both roles, recovery armed and unarmed,
zero input, the observed quiet samples, levels just below and at the floor,
normal signal level, and full scale. These are component boundaries, not a
claim of a complete call history. Live captures provide separate reachability
evidence.

The differential build must reproduce the original reset. The normal build
must reject quiet input and still accept the same prefix at signal level:

```sh
make period T=t_v34info0carrier
make check-info0-carrier
```

Validation passed with the Gentoo GCC 3.4.2-r2 reconstruction toolchain:
`make phase` reported 393 differential tests passed and zero failed, including
2,592 checks in the new reproduction test. The structural gate passed with
285 mutation suites, 10,042 mutations and zero skipped. The corrected normal
fixture passed 192 checks; `make check64` passed for both configurations.
The six existing mutations of this guard retain their faults and have been
re-anchored to the qualified condition.

## Live validation

Tests on 17 October 2026 used an Oli'Net V92 Ready (`56000 V5.015DS`) on VG204 port 0/2
and a Courier V.Everything on port 0/0. The media path used Asterisk and
G.711 A-law. D-Modem retained `IODELAY` 48. Calls used maximum positive DSP
debug level (`2147483647`), plaintext diagnostics, and no EC or handshake
observer. Serial hardware flow control was disabled and the hardware modems
used `AT&D0`; these console cables may not carry DTR.

| Call | Result | Exact bytes in each direction | Carrier hold |
| --- | --- | ---: | ---: |
| Olinet originates, V.34 forced, corrected build | Passed | 290 | 5 s |
| Olinet originates, V.34 forced, repeat | Passed | 1,058 | 10 s |
| Olinet originates, automatic modulation | Passed | 1,058 | 10 s |
| Courier originates, corrected build | Passed | 1,058 | 10 s |
| Olinet originates, V.34 forced, unchanged original control repeated afterwards | Failed in phase 2 | No data connection | Not applicable |
| D-Modem originates, Olinet answers, V.34 forced | Failed after phase 2 | No data connection | Not applicable |

The three successful Olinet calls reported 33,600 bit/s towards D-Modem and
28,800 bit/s towards Olinet. Before the correction, original and ordinary
reconstructed Olinet controls each transmitted 178 local INFO0 frames without
receiving a recovery acknowledgement. The corrected calls reached probing
and transferred exact data. An unchanged-original control repeated after the
successful corrected calls failed again with 178 local INFO0 frames. The
corrected repeat transmitted one local INFO0 frame.

The reverse-direction call reached `TX_L1`, `TX_L2`, receiver training and
`JTXMIT`, then returned `NO CARRIER`. That later failure remains unresolved;
the present results establish the phase-2 correction and successful
Olinet-originated calls, not reliable operation in both directions.
