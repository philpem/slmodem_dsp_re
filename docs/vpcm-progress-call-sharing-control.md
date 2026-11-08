# Progress retrain-call sharing control

This bounded investigation starts from `cc6377b2c6352923e4ad9f092cb905d0d3c4738a`
and uses only Gentoo GCC 3.4.2-r2 from
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, with the canonical flags.
No source change is retained.

## Independent native witness

`VPcmV34Progress` occupies reference `0xb3c0..0xd02e` (7,278 bytes).
Its complete relocation/opcode inventory contains five calls to
`VPcmV34InitiateRetrain` and no tail jumps to that callee. The current
canonical function contains eight calls and no tail jumps. There are nine
source requests; the native code groups them as follows:

| Incoming blocks | Requested DP | Common call |
| --- | --- | --- |
| `0xcbcf`, `0xcbec` | `0x22`, `0x5a` | `0xcbe2` |
| `0xcdc7`, `0xcde4` | `0x22`, `0x38` | `0xcdda` |
| `0xcdf9`, `0xce16`, `0xce2b` | `0x22`, `0x5a`, `0x5c` | `0xce0c` |
| tone-detector continuation | zero | `0xb688` |
| waiting-user timeout | zero | `0xb97c` |

All nine requests therefore have native retained-call paths. The smaller
native call-site count alone does not establish a missing inline expansion.
Shared machine tails also do not establish the author's source spelling.

## One bounded control

The three source switches selected an `unsigned char requestedDp` and jumped
to a shared call inside their own switch. Their two, two and three DP values,
exit continuations and the two independent zero-request calls were preserved.
The type follows the callee's existing argument declaration. The complete TU
was compiled, without an attribute, register declaration or flag change.

| Measure | Baseline | Common-call control | Reference |
| --- | ---: | ---: | ---: |
| Non-padding instructions | 1493 | 1479 | 1581 |
| Longest structural run | 31 | 31 | — |
| LCS, denominator 1581 | 723 | 720 | — |
| Retrain CALL sites | 8 | 5 | 5 |
| Retrain tail JMP sites | 0 | 0 | 0 |

The control reaches the native call-site count but reduces overall LCS and
moves instruction count farther from the reference. It fails the stated
structural-improvement discriminator and is not adopted. This is evidence
against this source control, not proof that every source route is exhausted.
Among all 56 common native-named functions, only `VPcmV34Progress` changes;
all 30 baseline EXACT functions remain exact. No differential or full phase
gate was run for the discarded source; no behavioral-equivalence claim is
made from the compilation result.

## Next discriminator and evidence

A useful next witness must distinguish the original dispatch/continuation
structure beyond the known shared calls. Compare the incoming blocks and
live owners at the joins before trying a different source representation;
matching the five-call count by itself is insufficient. `VPcmV34Progress`
already captures engine, transmitter and receiver owners, so the separate
DIL owner recovery does not supply a mechanical missing change here.

The isolated worktree is `/tmp/slmodem-astra-progress`, branch
`investigate/astra-progress-tail`. `scratch/progress-baseline.cpp` and `.o`
preserve the control; `scratch/progress-common-request.cpp` and `.o` preserve
the tested candidate; `scratch/progress-control-evidence.json` records full
scores, all retrain edge offsets/opcodes and the complete-TU exact audit.
The source file was restored after the experiment.
