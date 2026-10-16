# Reconstruction debt

This is a bounded reading audit of the host adapters, V.34 layouts and PCM
session interfaces used by this guide, checked at base revision `47174bf3`
on 16 October 2026. It is not an exhaustive static-analysis report or a new
defect census. Six observations are recorded below; none is evidence of a
newly discovered failing public modem call.

“Confirmed” means the stated source representation or documentation conflict
was observed. “Needs measurement” means behaviour or reachability remains to
be established. These entries propose follow-up work; this documentation change
does not repair reconstruction source or relax a gate.

## MG-01: Duplicate phase 4 demodulator model

**Confirmed structural debt; high priority.**
[`V90SessionFlag.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/V90SessionFlag.h)
and
[`V90Phase4Demodulator.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/V90Phase4Demodulator.h)
both define `V90Phase4Demodulator`. The former models a prefix through an
embedded modulator; the latter describes the fuller object.
`python3 tools/onedef.py` reports **337 types in 171 files, one known
duplicate**, and names this exception in its `KNOWN` register.

Different translation units can therefore compile against different complete
class definitions. The prefix model is explicitly a lower bound, so its
`sizeof` must not be used as an allocation authority. Matching the prefix's
accesses does not establish consistency of the full type.

**Next step:** trace all inclusion and use sites, then test consolidation on
an isolated branch. Preserve the existing field offsets, emitted functions
and reference comparisons across the affected translation units. Close this
entry when one complete definition remains, `onedef.py` no longer needs the
exception, the period gate passes, and full-TU code-generation effects are
recorded. The existing header points to F7584 for earlier consolidation costs.

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

**Confirmed representation debt; medium priority.**
[`v34fsk.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/v34fsk.h)
retains `unmapped_*` spans and
[`v34hstx1_arms.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/v34hstx1_arms.h)
uses offset-based accessors such as `hs_get` and `hs_put`. Some offsets identify
known fields; other comments bound only a width or a particular reader/writer.
The receiver also retains a large `pad_2a8` region.

Generic state accessors intentionally serve several state words, so their
existence alone is not a defect. The maintenance hazard is losing the base,
width or evidence behind a particular offset, or assuming a pad is alignment.

**Next step:** select a bounded region with concrete readers and writers,
recover its field shape and owner, then name only what the binary supports.
Close each regional follow-up with live assertions, updated uses and mutation
anchors, period differential results and code-generation measurements. Do not
claim this entire entry closed because a raw-offset count decreases.

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

**Confirmed documentation debt; medium priority.**
The opening comment in
[`V34hshak.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/V34hshak.c)
describes `v34handshak` as unwritten in its account of earlier harness limits;
the same file now contains its implementation and `datapumpv34`.
[`V90SessionFlag.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/V90SessionFlag.h)
still says several allocations have not been found, while
[`V90Phase4Demodulator.cpp`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Phase4Demodulator.cpp)
records the phase 4 allocation and asserts the fuller size `0x351c`.
[`VPcmFloModem.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/VPcmFloModem.h)
also contains an early partial-reading account that must be reconciled with
the subsequently added members and constructor.

These comments preserve useful history, but a maintainer can read their
present-tense claims as live reconstruction gaps. The README likewise calls
the reconstruction “unfinished” in a paragraph about linking the blob, despite
its status section recording completed symbol reconstruction.

**Next step:** annotate or rewrite each introduction around its current
contract, preserving finding references and explicitly labelling historical
constraints. Check measurement claims against tools rather than copying old
counts. This PR adds a current reading path; it leaves that wider comment
audit as follow-up work.

## MG-06: Queue-path coverage claim needs reconciliation

**Confirmed evidence-documentation gap; coverage status needs measurement.**
[`vpcm.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/vpcm.c)
labels `pending > 0` unexercised because the normal 48-sample host call leaves
no remainder after four-sample quantisation.
[`t_vpcmrun.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/test/unit/t_vpcmrun.c)
documents a whole-call comparison at that normal block size. This reading
audit does not establish whether all other tests now cover fragmented input.

The queue rebinding and subsequent compaction are real code paths. A green
whole-call comparison at a multiple-of-four count does not establish their
coverage. Conversely, an old “unexercised” comment is not enough to declare
them untested today.

**Next step:** inspect the complete relevant fixture set, then measure
remainder occupancy and the queue-backed reads during valid chunked processing.
Use distinct reference/reconstructed input histories and compare samples,
statuses and payload observers. Close with a named fixture and its exercised
denominators, or explicitly document the host boundary that excludes this path.
