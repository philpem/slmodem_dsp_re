# Reconstruction debt

The initial bounded reading audit used `47174bf3` on 16 October 2026.
The follow-up fixes start from current master `ac7f28e6`; that revision already
contains repairs absent from the initial snapshot. The statuses below distinguish
new fixes, corrections to superseded findings and the evidence-limited residual.
This is not an exhaustive static-analysis or defect census, and none of the
initial observations established a newly failing public modem call.

| Finding | Follow-up status |
|---|---|
| MG-01 | Resolved: single complete phase 4 demodulator definition |
| MG-02 | Closed: receiver ownership was already reconciled |
| MG-03 | Partial: final raw state reader removed; supervisor helpers tracked in #286 |
| MG-04 | Resolved: existing refusal behaviour verified and contract clarified |
| MG-05 | Resolved: stale introductions corrected |
| MG-06 | Resolved: remainder paths exercised in chunked connecting calls |

“Resolved” describes the bounded finding, not project-wide reconstruction
completion. Original-binary behaviour remains the compatibility authority;
the changes do not relax a differential, structural or code-generation gate.

## MG-01: Duplicate phase 4 demodulator model

**Resolved: one complete definition.**
[`V90SessionFlag.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/V90SessionFlag.h)
now includes the full
[`V90Phase4Demodulator.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/V90Phase4Demodulator.h)
instead of defining a second prefix model. The session-flag fixture reserves
the complete object plus slack, and `tools/onedef.py` carries no exceptions.
Its current check reports 335 types in 168 files and zero duplicates.

The consolidation preserves all 300 complete period-compiled source objects
byte for byte, as well as their complete partial-link object. The period gate
passes 388 test binaries with zero failures; the byte-identity and instruction
ratchets pass without losing exact symbols. See
[the consolidation evidence](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/phase4-type-consolidation.md)
for the include graph, compiler configuration, assembler identity and checks.
This removes the type inconsistency without claiming that the replacement
object is already identical to the original blob.

## MG-02: Overlapping V.34 root and receiver views

**Closed: the guide described a superseded representation.** Rechecked at
`ac7f28e6` on 16 October 2026. The source already has one owner for this
region: [`v34fsk.h`](https://github.com/philpem/slmodem_dsp_re/blob/ac7f28e6/include/dsplib/v34fsk.h)
embeds `struct v34_receiver receiver` at root `+0x264`, and
[`v34recv.h`](https://github.com/philpem/slmodem_dsp_re/blob/ac7f28e6/include/dsplib/v34recv.h)
defines its complete `0x79c`-byte extent. Issue #260's wave 8 moved the
root's fields into that owner; wave 9 modelled the shared burst storage.
The former opening `pad_000[0x120]` is absent.

Use the following translations when tracing callers:

| Member | Receiver offset | Root offset |
|---|---|---|
| `rxq` | `+0x000` | `+0x264` |
| `rxq_ring_tail` | `+0x010` | `+0x274` |
| `burst` | `+0x10c` | `+0x370` |
| `short_380` | `+0x11c` | `+0x380` |
| `short_382` | `+0x11e` | `+0x382` |
| `timing` | `+0x2a8` | `+0x50c` |
| `equalizer` | `+0x3cc` | `+0x630` |

The root header asserts the embedded receiver's offset and size, plus nested
member offsets including `rxq`, `burst`, `short_382`, `timing` and `equalizer`.
The receiver header asserts its own member offsets and extent. For example,
`V34RX.c` passes `&obj->receiver.timing` to `V34TimingFiltersInit` and uses
`obj->receiver.burst` as the sample window; `V34hshak.c` reads
`obj->receiver.rxq.count` and writes `obj->receiver.short_382`.
These are accesses through the same embedded object.

No reconstruction change is required for this finding. The remaining
`unmapped_0114` span and neutral field names record narrower evidence limits;
they do not establish conflicting root and receiver definitions. Future field
recovery must preserve both offset bases and the existing differential checks.

## MG-03: Offset access remains part of the handshake model

**Partially resolved: obsolete claims corrected and the remaining raw state
reader removed.** At `ac7f28e6`, issue #260 has already replaced receiver
`pad_000` and `pad_2a8` with recovered fields/subobjects, and `hs_put` is
already gone. The three remaining `hs_get` calls read the known signed-short
`ratecfg.rxbits` field. They now use that member directly; the unused
accessor and `DP_FAA98` macro are removed.

The minimal substitution leaves the complete period-compiled `V34hshak.c`
object unchanged across all 54 emitted functions. It adds no guessed field,
name or type and does not change comparison promotion or counter wrap.

A bounded broader typed rewrite was also measured, then withheld: only the
non-exact `datapumpv34` changed, from 988 to 919 bytes against the original's
1028, without an independent recovery of the changed counter/helper lowering.
The existing `dp_rxget`/`dp_rxput`/`dp_run` group therefore remains open in
[issue #286](https://github.com/philpem/slmodem_dsp_re/issues/286) under
the F8044/F8045 evidence limits. Unmapped storage with unsupported meaning
also remains neutral. This entry is not closed by a reduced offset count.
See [the two-cell record](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/maintainer-offset-access.md)
for the source boundary, full flags and the next evidence needed.

## MG-04: Wrapper preconditions need an explicit boundary

**Resolved: source fix already landed; contract clarified.** The initial audit
described `47174bf3`. At the follow-up baseline `ac7f28e6`,
[`dp_wrapper.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/dp_wrapper.c)
already uses literal converter creation and sequential null checks. An
unsupported unequal pair returns NULL before reverse-converter dispatch.
The repair landed in `67e75151` through PR #284, before the guide was merged.
The stale finding must not prompt another source change.

[`t_dp_wrapper.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/test/unit/t_dp_wrapper.c)
contains six positive unequal-rate refusal cases: 7200/8000, 8000/7200,
16000/9600, 9600/16000, 44100/48000 and 48000/44100. Each checks both reference
agreement and the reference's explicit refusal. Supported-rate streaming,
ragged input and equal-rate cases provide positive controls. The original
failure and its correction are recorded in
[the constructor fidelity results](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/service-constructor-fidelity-results.md).

The header now distinguishes the constructor's recovered rejection checks
from streaming preconditions: positive rates and fragment, representable
fragment scaling, an integral host fragment and sufficient ring capacity.
It also describes the callback's two private-state hops correctly. Negative
values and oversized scaled fragments are not newly accepted streaming
fixtures, and no additional validation has been inserted into reconstruction
source. The contract and current behaviour now agree; arbitrary-input
hardening would be a separately scoped compatibility change.

## MG-05: Historical comments overstate current missing work

**Resolved: current contracts separated from historical recovery notes.**
The introductions to `V34hshak.c`, `VPcmFloModem.h` and `VpcmFloModem.cpp`
now describe their implemented handshake, session and lifecycle work. The
earlier fourteen-support-function scope and displacement-only object floor
are explicitly historical. The PCM class's complete `0x7f68` extent is tied
to its lifecycle fixture; the embedded `V90Modem` extent and offset assertions
remain documented with their owning files.

MG-01's consolidation also replaced `V90SessionFlag.h`'s obsolete partial-class
introduction and the duplicate-specific inclusion warning in its consumer.
The README now distinguishes completed source-symbol reconstruction from
validation and final-object byte identity, limits blob patching guidance to
deployments that use it, and names `make phase` as the reconstruction gate.

Finding references and useful historical evidence are retained. These are
comment/prose changes; no field declaration or executable statement changes.
The complete period source-object comparison verifies that they introduce no
code-generation changes. Future counts and completeness claims should continue
to come from the relevant tool and stated revision.

## MG-06: Queue-path coverage claim needs reconciliation

**Resolved: constructor-created chunked-call coverage.**
[`t_vpcmrun.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/test/unit/t_vpcmrun.c)
now repeats its four reference/reconstruction endpoint combinations with each
48-sample wire block partitioned into calls of 1, 2, 3 and 42 samples. V.8
negotiates the call; the fixture does not plant queues or handshake state.
Each run has separate writable samples, runtime state and payload observers.

The added group passes 352,093 exact checks over 128,000 chunk calls. Entry
queue occupancies 0, 1, 2 and 3 each occur 32,000 times; 64,000 calls process
no full group, 176 use the initial mute path and 63,824 process active samples.
Returned samples, per-chunk statuses, active queue contents, progress and
captured receive transcripts agree with the blob. The original 310 checks
remain unchanged, and the full period suite stays at 388 binaries, all passing.

A temporary pending-sample corruption caused 16,000 failures in the new
group while the original groups remained green. It was removed before the
retained gate. This establishes observation of the remainder path, rather than
merely a clean test result. The source's obsolete unexercised-path comment has
been replaced with a pointer to the coverage evidence.

See [the queue-coverage record](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/vpcm-queue-coverage.md)
for fixtures, exact denominators and controls. The result establishes reachable
datapump-component processing; the stock host still calls with 48 samples, and
this is not evidence of digital-side V.90/V.92 connectivity.
