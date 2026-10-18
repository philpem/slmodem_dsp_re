# V22 FSE configuration capture validation

`V22_FSE_init` now captures both configuration pointers before publishing either
pointer into the state. The native body loads `cfg+4` and `cfg+0` before either
pointer store; the previous source published I before reading Q. This is a
supported partial reconstruction, not a claim of byte identity or a runtime bug.

F11741 already measured this endpoint and withheld it for strict byte proof.
The prior report did not identify contradictory native evidence or execute runtime
or mutation validation. This review replays only its baseline and copy endpoint;
no register, declaration, alias, or compiler-option palette was reopened.

## Replay and provenance

Source and original mutation row are pinned to `6b16d64d90891138f9891067bb3e41cad136853d`. The initial frozen lead
at `dc4161c89` has identical V22 source. `build/v22-fse-capture293/frozen/` holds
the pre-adoption canonical object and complete `.build-config`; `validation-pin.json`
records their hashes and the source hash. The driver reads Git source rather than
mutable generated source. Post-adoption default replay reproduced both controls.

```sh
python3 tools/recovery_v22_fse_capture.py
python3 tools/recovery_v22_fse_capture.py --audit
python3 tools/recovery_v22_fse_capture.py --period
python3 tools/recovery_v22_fse_capture.py --mutant
make period T='t_v22_fse t_v22rate' J=4
```

The actual copy-cell compiler invocation, including the complete retained flags
and the shared helper's final reproduction define, is:

```sh
cd /work/v22_fse/copy && export PATH=/usr/i386-pc-linux-gnu/gcc-bin/3.4:$PATH; exec gcc -c -O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -I/src/include -D__SIZEOF_POINTER__=4 -include /src/tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS -DDSPLIB_REPRODUCE_BUGS -v -save-temps -da -dP -DDSPLIB_REPRODUCE_BUGS -o /work/v22_fse/copy/candidate.o /work/v22_fse/copy/v22_fse.c
```

`results.json` records the complete Docker argv, configuration, header hashes,
compiler and assembler identities, source/object hashes and every body verdict.
The image is the canonical Gentoo GCC 3.4.2-r2 image; no host-generated dump or
profile change participates.

## Complete-object and native checks

| Measurement | Baseline | Capture |
|---|---:|---:|
| Emitted functions | 4 | 4 |
| Exact original functions | 2 | 2 |
| Init size versus native 372 bytes | 379 | 372 |
| Init verdict | SIZE 7 | BYTES 5 |
| Changed bodies | 0 | 1 |

The other three bodies, data, BSS, public bindings and relocation consumers are
unchanged. The ordering detector records both configuration reads and both pointer
stores: native and capture satisfy the ordering, while the baseline is the known
negative control. This is an instruction witness, not proof of unique source.

The five remaining differing bytes encode five register operand changes: the
configuration argument and its two reads use ESI instead of native ECX; the fresh
argument and its TEST use ECX instead of native ESI. Every other canonical
instruction agrees. `audit.json` retains full instructions; `residual.json` retains
the exact five-line operand difference. No register fitting was attempted.

## Differential and mutation boundary

Both baseline and capture pass `t_v22_fse` (1,023 checks) and `t_v22rate`
(20,547 checks), two fixtures and 21,570 checks per cell. The latter includes
constructor-created datapumps warmed through the reference receive chain before
`ResetRx` splits source/reference execution. That real reset passes the equaliser
as its own configuration. This does not establish arbitrary partial-overlap
histories, and none was invented.

The existing I/Q swap fault is preserved by changing its anchor operands from
`cfg->` to `copied.`. All 10 `v22fse` and five `v22recv` source-bound anchors
remain unique. Its actual period mutant fails 298/1,023 FSE checks (199 fresh,
99 reuse) and 40/20,547 rate checks, including the warmed ResetRx group. No
mutation was reclassified or weakened. Production is restored and passes both
fixtures; its period object is raw-identical to the independently audited copy
candidate (`production-audit.json`).

Parent integration owns the full reconstruction gate. These focused results do
not claim modem end-to-end operation, portability, fuzzing, or whole-object identity.
