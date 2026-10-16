# Echo append: supported loop shapes, no exact preimage

Baseline `5b552950`, Gentoo GCC 3.4.2-r2 with the retained complete production
flags and mandatory `DSPLIB_REPRODUCE_BUGS`. The original
`V92EchoCanceller::updateEchoHistory` is 244 bytes. Domains were recorded before
compilation: [cursor/length](residual-echo-append-domain.md) and
[exit/count timing](residual-echo-append-exit-domain.md).

Nine complete-TU cells produce 135 common body verdicts. Both baseline objects
reproduce raw, as does the repeated combined control. All fourteen bystanders,
bindings, imports/exports, allocated nontext data, BSS and nontext relocations
remain unchanged. Every cell retains the same six exact bodies out of fifteen.
No source or header change is adopted; production remains 1079/1852 exact.

| Fast input/count loop | Slow length update | Bytes | Strict verdict |
|---|---|---:|---|
| Indexed ascending | Test length+1; increment else |225| SIZE 19 |
| Advancing input/countdown | Existing |261| SIZE 17 |
| Existing | Preincrement; compact from length-1 |209| SIZE 35 |
| Advancing input/countdown | Preincrement |245| SIZE 1 |

The original witnesses are an advancing input pointer before the opaque
FloatARMA call, a remaining-count decrement, and a separate original count
retained for the final length addition. On the slow path it increments length
before comparison and starts compaction at the preceding sample. The combined
control recovers these shapes and the 0x1c frame, but has 81 nonpadding
instructions against the original's 80, with different register allocation and
loop placement. A one-byte size gap does not establish a source recovery.

The follow-up crosses a common if/else exit with initialization of the remaining
counter inside the nonzero guard. Original machine code has one cleanup path
and copies the counter after the guard; neither observation uniquely identifies
source syntax.

| Common exit | Guard-owned counter | Bytes | Strict verdict |
|---|---|---:|---|
| No | No (combined repeat) |245| SIZE 1 |
| Yes | No |245| SIZE 1 |
| No | Yes |226| SIZE 18 |
| Yes | Yes |226| SIZE 18 |

Changing to a common exit is raw-object-inert at both counter timings. The
guard-owned counter changes output but misses. These two finite crosses are
closed; do not expand nearby declarations or register-color spellings from the
size result. A new experiment needs an independent witness.

Reproduction: `tools/residual_echo_append_reproduce.py`, then
`tools/residual_echo_append_exit_reproduce.py`, then
`tools/residual_echo_append_audit.py`. The audit reports its nine-cell denominator
and checks unique target ownership in nine RTL stages per cell. Results live in
`build/residual-echo-append*/` and `build/residual-echo-append-audit.json`.
New tools compile and the diff check passes. No candidate runtime validation,
mutation execution or fuzzing is claimed. The production source is unchanged,
so its previous period differential gate remains 388 passed / 0 failed.
