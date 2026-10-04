# Callprog callback order and conditional validation reread

The initial two-read seed failed the valid Create → Dial lifecycle fixture:
10 of 62 strict transcript checks disagreed with the blob. Detailed period
traces identified two further source boundaries. Neither was a fixture or
tolerance defect.

The blind diagnostic parameter-29 getter and print execute **before** the
mainline parameter-29 read and timeout store. The blob branch at 0x7a7c0
enters the cold block at 0x7a975, then rejoins at 0x7a7c6. Static addresses do
not describe execution order. In the wait branch, parameter 34 is
GetDialToneValidationTime: the first read bounds `(value + 9) / 10`; when this
exceeds two, the branch at 0x7a9aa makes a second read and recalculates it.
Parameter 45, GetDialToneWaitTime, is unchanged.

## Declared domain

Five complete-TU cells: unchanged 93d7eee1 baseline, then blind diagnostic
before/after the timeout read crossed with conditional validation reread
absent/present, starting from the earlier independent no-answer/blind getter
seed. No other source changes. The complete retained profile, bug define,
unchanged headers and actual Gentoo GCC 3.4.2-r2 assembler are recorded in
`build/gcc3-batch20-callprog-callback/results.json`.

## Object results

| Diagnostic first | Validation reread | CALLPROG_Dial bytes | Blob verdict |
|---|---|---:|---|
| Original baseline | Original baseline | 955 | SIZE 139 |
| No | No | 1021 | SIZE 73 |
| No | Yes | 1094 | BYTES 423 |
| Yes | No | 1030 | SIZE 64 |
| Yes | Yes | 1094 | BYTES 422 |

All five TUs retain 3/6 exact symbols, without gains or losses. Only
CALLPROG_Dial changes. All 17 named data objects, symbol metadata,
nontext sections and relocation targets are preserved; format-string section
order may change, with the same length and NUL-delimited string multiplicities.
Initial RTL getter counts are 9/11/12/11/12. The unchanged baseline reproduces
its archived raw object, and the restored production period object is raw
byte-identical to the final controlled candidate. Equal size is not identity:
the final 422-byte residual remains open.

Reproduce with:

```
python3 tools/gcc3_batch20_callprog_callback_reproduce.py --domain docs/batch20-callprog-callback-order-controls.md
python3 tools/gcc3_batch20_callprog_callback_audit.py
```

## Deciding lifecycle control

`test/unit/t_callprog_create.c` exercises eight valid Create → Dial histories:
debug levels 0/1/2/3 crossed with blind/wait. It compares the entire Dial
host-read/printed sequence and callback counts, retaining the original
Create/Dial/Delete printed transcript checks. Reference anti-vacuity checks
establish parameter-30 reads 5/6, parameter-29 reads 0/1/2, and parameter-34
reads zero in blind mode or two in wait mode with validation 50. All 70 checks
are strict; no filtering or tolerance is introduced.

The targeted command is `make period T=t_callprog_create J=4`.
The original 93d7eee1 source fails 12/70 trace checks (0 passed, 1 failed test).
The restored final source passes 70/70 (1 passed, 0 failed test). Intermediate
seed failure 10/62 is preserved independently. Logs are
`/tmp/batch20-callprog-old-source-control.log`,
`/tmp/batch20-callprog-final-restored.log`, and
`/tmp/batch20-callprog-trace-detail.log`. The parent runs the complete batch
phase after integration. No mutation or fuzzing was run.
