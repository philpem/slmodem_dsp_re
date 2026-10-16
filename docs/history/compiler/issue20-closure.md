# Issue #20: ownership and ordering closure

This report supersedes the pending-work status in the earlier #20 milestone
documents. PR #95 completes the ownership/ordering review for merge before
byte-fidelity refinement. It does not claim that every original owner or every
original emission order has been recovered.

## Scope and evidence

The machine-readable registers are `issue20-owner-ledger.json` (74 historical
unanchored inputs and their dispositions) and `issue20-order-ledger.json`
(268 reviewed baseline inputs, 277 order edges). The current partial-link
manifest contains 275 inputs: 203 ordering candidates and 72 retained without
an identified original position. A filename-policy candidate is not direct
LOCAL-symbol provenance. Unknown ownership remains an ownership uncertainty,
not a silently reclassified byte-fidelity task.

Earlier batches restored the Fdsp and session-flag groups. This batch merges
the VpcmFloModem constructor and its three LOCAL tables into VpcmFloModem.cpp,
and separates the mixed V22/v22mod functions and locals into V22.c and
v22mod.c. Dependent declarations and mutation routes follow the definitions.
The V.22 BSS group now has the reference-relative ordering.

Eight fax RX/TX files retain five lower wrappers each. Their filename grouping,
and the SMC encoder group placed in Smc.c, are naming-policy candidates, not
proven LOCAL owners. Original ownership of the uppercase fax APIs remains
unidentified. The ledger records those limits rather than inventing owners.

Ordinary definition-order corrections are retained where compatible with
dependencies and the exact-function set. ABI clones, dependency constraints,
and rejected controls are recorded separately. Completion means this review
and disposition are complete, not that all reference-address descents vanished.

## Rejected controls and retained uncertainties

- Co-locating 46 uppercase fax API definitions with the lower wrappers lost
  20 previously exact wrappers: original tail calls became expanded bodies.
  That co-location was reverted; the lower-wrapper file split remains.
- V90Demodulator definition reordering lost two exact constructors and was
  reverted. Helper-constrained extraction/order attempts in other units were
  also rejected rather than forced with source attributes or compiler flags.
- The corrected VpcmFloModem constructor placement preserves qcLineVerification.
  It is before getV90JaBits, not first in the file. Earlier malformed extraction
  controls do not establish an isolated constructor-placement effect.
- V22.c emits TONEv22INIT_CFG before TONEv22_CFG under both tested declaration
  orders; the reference emits CFG first. Canonical relocations confirm that
  both source and reference use CFG first and INIT second in V22FP_create.
  Swapping names or call sites would therefore be an unsupported correction.
  Ownership is resolved; physical table emission order remains unresolved.
- Restoring source with preserved timestamps initially reused stale objects.
  Those intermediate results were rejected. All source objects were forced
  fresh before the final measurements and final phase gate below.

## Fresh period validation

The reviewed `/tmp/issue20-gentoo` runner rebuilt the candidate partial link
and ran `make phase J=6` using the default Gentoo GCC 3.4.2-r2 image.
The final run exited zero: **375 period tests passed, 0 failed**, and all
structural checks passed. Mutation anchor coverage is 272 suites and 10,041
mutations; this is an anchor check, not a fresh mutation execution result.

The compiler banner matches the reference:

```text
GCC: (GNU) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)
```

The existing period profile and DSPLIB_REPRODUCE_BUGS remain in force. No
compiler-profile change or modern-compiler source workaround is retained.
The modern FDSP issue is tracked separately in #96. Modern mutation snapshot
validation remains in the explicit test/portability gate, while mutation
route/anchor validation remains structural. No fresh modern result is claimed.

## Independent partial-link measurements

Before is committed cf7af92e; after is the fresh candidate source rebuild.

| Measurement | Before | After | Denominator/reference |
| --- | ---: | ---: | ---: |
| Exact functions | 829 | 830 | 1,852 |
| Exact function bytes | 80,002 | 80,030 | 720,125 |
| Positioned matching bytes | 57,193 | 68,456 | 943,398 |
| Exact relocation records | 987 | 974 | 18,317 |
| Ordered exact relocation records | 978 | 966 | 18,317 |
| Exact symbol records | 263 | 299 | 2,907 |
| Ordered exact symbol records | 262 | 298 | 2,907 |
| Candidate .text bytes | 684,488 | 684,472 | 728,304 |
| Aggregate content deficit | 45,748 | 45,764 | bytes |
| Candidate NOBITS bytes | 2,812 | 2,840 | 2,836 |
| Inputs | 268 | 275 | |
| Ordering candidates | 194 | 203 | |
| Unanchored inputs | 74 | 72 | |

All **829 baseline exact members survive**; the sole gain is dp_v23_exit.
The stored exact-function ratchet also passes. Defined FUNC records remain
1,922 before and after, with no added/removed function names or binding
records. Shared-name binding agrees for 2,443/2,443 names, with zero
disagreements; reference-only 75 and candidate-only 130 remain unchanged.
The partial comparison has 92 shared sections (99 candidate, 92 reference),
67 exact descriptors, 59 ordered exact descriptors and 60 exact contents.

The relocation and content-deficit regressions are disclosed independently;
positioned-byte improvement is not a substitute for these dimensions. The
strict whole-object verdict remains **DIFFERENT**.

## Merge boundary and remaining work

The ownership/order implementation and review checklist is ready for merge in
PR #95. The last #20 step is merging it before starting byte refinement.
Unidentified owners and constrained emission orders remain explicitly in the
ledgers for new evidence, not speculative follow-up edits.

Issue #6's ordering/measurement infrastructure is delivered and remains closed.
After #95 merges, investigate remaining code/data/relocation/layout differences
and the original profile under #22's experiment discipline. Do not interpret
this ownership milestone as full-object binary identity or a portability claim.
