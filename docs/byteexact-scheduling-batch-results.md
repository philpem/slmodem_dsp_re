# Ten-function publication and return-contract batch

PR273 merged at240481e6 with both CI checks green. This isolated branch started
at1057/1852 exact functions,114842 exact original bytes. PR263's V34 source,
headers, tests, shared Makefile and mutation snapshots remain untouched; its
scope was observed at ef22230e. No #22 writes or fuzz/mutation execution.

## Complete source recoveries

| Functions | Gain count | Original bytes | Source boundary |
| --- | ---: | ---: | --- |
| cid_reset |1|118| Clear samples_fill after the conditional automatic-mode update |
| V90BitsToSymbol::reset |1|108| Guarded unsigned division feeding one common extraSymbols store |
| v17/v21/v27/v29 transmit and receive process adapters |8|496| Preserve and return wrapped modem status through the count relay |

The fax dispatcher already consumes an int callback status. Its return merges
status bits from the adapters, so the old void declarations and compensating
casts hid a real return contract. All eight adapters now match62bytes each.
Their table entries have matching prototypes; removing the eight obsolete casts
leaves the complete dispatcher object raw-identical. Header comments retire the
old unspecified-EAX explanation and the stale V27-unwritten claim.

CID constructor cid_create changes through its inlined reset and remains
nonexact BYTES70; this changed bystander was reviewed, not hidden by the gain.
All other emitted bodies outside the ten winners remain unchanged. The
conditional and guarded-result BitsToSymbol forms yield the same raw object;
the conditional source is retained for readability. This is uniqueness within
the declared finite families, not a unique-author spelling claim.

## Combined production proof

All300 production objects built on retained Gentoo profile with zero failures.
Six changed objects raw-repeat their independent full-TU winners;294objects
and the complete .build-config remain raw-identical. Symbol metadata/binding,
allocated data/BSS and canonical nontext relocations are unchanged. Eleven
bodies change: ten newly exact functions plus the reviewed CID constructor.

Strict census: **1067/1852 exact**, **115564 exact original bytes**; exactly the
10names above gained,0lost,1852denominator unchanged. This is+722originalbytes;
not a claim of complete linked-object identity. Production proof tool:
`tools/byteexact_scheduling_batch_audit.py`; artifacts
`build/batch-final-byteident.json` and `build/batch-production-audit.json`.

First combined phase ran388period differential tests/0failures, but five reset
metadata anchors detached when the if/else became a conditional expression.
Retargeted the same signed-division/numerator/guard/zero-result/done-store
mutations in test/mutations/v90bits.json. Static-only maintenance; no mutation
execution or snapshot refresh. Corrected `make phase J=4` exits0:388period differential passes/0failures, all
structural checks green. Static census285suites/10038anchors,0detached or
non-unique. The two period runs are each388fixtures, not776distinct tests.
No modern portability claim. New-tool syntax22/22 and whitespace checks pass.

## Bounded investigation and negative controls

85valid archived full-TU cells,596emitted-body comparisons: services28/125;
C++16/207; fax37/248; scheduler2/8; FSE configuration2/8. These include repeated
controls and header-consumer cells, not85distinct source candidates. Agent
audits are independently rerun in this branch with live object hashes/verdicts.
Raw baselines, exports/binding, data/BSS and nontext relocations pass.

Services: V23 count/publication, FDSP helper/owner and DTMF predicate/lifetime
families close without source adoption. C++: Phase3 code-segment topology,
Precoder clear expansion and SD detector pointer/result families remain misses.
Fax CRC narrowing and V21 control flag-capture families remain misses. Invalid
V21 shifted-byte and precompile generator-refusal runs are preserved/excluded,
not interpreted as evidence. Complete domain reports accompany the tools.

Verbose scheduler instrumentation raw-repeats both V8 controls. Actual Gentoo
load table priority35/35 and outgoing edge4/5 explains why sample capture wins
ready-queue sorting. Official GCC3.4.2 ranking code supplies a matching mechanism;
its release source is not claimed to be the complete Gentoo patch reconstruction.
This does not solve original allocation or license forced dependencies/registers.
FSE config-copy relocation alone is whole-object raw-inert. Both families close
without production changes. See docs/batch-scheduler-trace-domain.md and
batch-fse-config-boundary-domain.md.

## Replay and next work

Full domain tools require pinned baseline objects/config and --historical-headers
after the shared header changes. The fax overlays explicitly read the pinned
header with gitshow, preserving post-adoption replay. All artifacts are on ZFS.

Next useful broad pass: inspect callback tables whose int consumers still cast
void-returning reconstruction adapters, and source branches that duplicate one
final field store. Require direct caller/return or original common-store evidence
before compiling. These levers produced10complete gains here; regalloc-only and
closed near-miss families should not be reopened by equivalent spellings.

Keep V8 scheduler work as a separate evidence track: its original coefficient
pointer is reused for sample-address formation, potentially enforcing an
anti-dependency absent from our allocation. Establish source lifetime/ownership
or original compiler-profile evidence before changing code to mimic register use.
That explanation is bounded; byte identity alone does not recover original RTL.
