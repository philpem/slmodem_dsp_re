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

**Confirmed incomplete representation; high priority.**
[`v34recv.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/v34recv.h)
begins its receiver view with `pad_000[0x120]`. The receiver base is root
`+0x264`, where
[`v34fsk.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/v34fsk.h)
models `rxq`, its ring tail, an unmapped span and `short_382`. Root `+0x382`
is receiver-relative `+0x11e`, inside that apparent pad.

Those bytes are live storage represented differently by two views. Removing
the pad as “unused” would destroy the map; inventing a second independent
receiver member would conceal the overlap.

**Next step:** inventory reads/writes with both bases stated, and establish
which shared subobject or explicit view represents the original ownership.
Retain neutral names until meaning is supported. Close when shared live bytes
have a consistent representation with offset assertions, lifecycle differential
coverage and measured full-TU effects. This is a layout recovery task, not
permission to repack the object.

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

**Confirmed contract mismatch; behavioural significance needs measurement.**
[`dp_wrapper.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/dp_wrapper.h)
describes failure when a needed conversion cannot be built.
[`dp_wrapper.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/dp_wrapper.c)
creates converters only when its rate lookup returns a non-negative mode.
For unsupported unequal rate pairs, both modes can be `-1` and the allocation
failure test is skipped. Creation can then return a wrapper without either
converter. The guards reject zero arguments but do not establish positive
rates or a positive, bounded *scaled host fragment* for arbitrary inputs.

This is evidence that the wrapper is a historically constrained interface.
It does not prove that the shipped host requests such values, or that adding
modern validation would match the blob. The guide documents the supported
conversion pairs rather than promising an arbitrary-rate service.

**Next step:** audit valid constructor callers and existing `t_dp_wrapper`
fixtures; compare disputed inputs against the blob with appropriately sized
buffers. State which inputs are public-host reachable and which are synthetic.
Close when header and guide contracts match measured behaviour and caller
preconditions. Any behaviour change requires separate compatibility evidence.

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
