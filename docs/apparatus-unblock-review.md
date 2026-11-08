# Proof boundaries in the residual reconstruction

Two apparatus investigations at `60804df5b37956fc20fccfa4dfb9ded282fd9711`
separate unsupported comparisons from actual source differences. They do not
claim new EXACT source functions or alter the conservative ratchet. A Playbook
review also narrows conclusions that exceeded their experimental domains.

The frozen build contains 300 objects from 300 sources, all compiled by
Gentoo GCC 3.4.2-r2 with the retained profiles and `DSPLIB_REPRODUCE_BUGS`.
Its census is 1,852 functions: 1,144 EXACT, 36 REGALLOC, 86 BYTES and 586 SIZE;
zero RELOC, UNRESOLVED and NODATA. No reconstruction source was changed.

## Separate the questions before reopening a function

| Observation | What it establishes | Next discriminator |
| --- | --- | --- |
| Equal opcode/bucket projections | Equality of that projection | Compare dependencies, predicates, value origins and member associations. |
| An exact out-of-line helper | Its final function body matches | Inspect frontend cost and visibility separately; do not infer an original keyword. |
| A table prover refuses | A proof obligation is unsupported | Record the first refusal and inspect later obligations and independent byte differences. |
| A worst-copy COMDAT discrepancy | At least one emitted copy differs | Inspect every copy and the body actually present in the supplied link. |
| A changed call census under a diagnostic option | That option affects code layout or optimization | Locate the first changing pass; retain production-profile source as a separate hypothesis. |
| No hits in an enumeration | No hit in that declared family/profile | Name the untested source properties and interactions. |

Neither a smaller gap nor a larger LCS removes these obligations. Keep useful
losing controls and partial structural candidates, with their validation
limits, rather than treating a score as a proof or a stopping rule.

## A selected-body view alongside the conservative census

[PR #334](https://github.com/philpem/slmodem_dsp_re/pull/334) adds
`tools/toolchain/linkedcopies.py`. The inventory covers all 32 repeated
function names / 66 definition records among 1,915 input function records.
All repeated names in this build are weak external GNU-linkonce definitions.

| View over the 32 names | EXACT | REGALLOC | BYTES | SIZE |
| --- | ---: | ---: | ---: | ---: |
| Conservative per-copy | 26 | 2 | 2 | 2 |
| Supplied recovered-order link | 29 | 0 | 1 | 2 |
| Reversed-order control | 26 | 2 | 2 | 2 |

Three Scrambler bodies are EXACT in the recovered-order link while their
conservative grades remain non-exact. A fourth constructor changes ten byte
positions under reversal while remaining BYTES; a grade-only list misses
that observation. The two window templates remain SIZE in every copy/order.

All 32 selected symbols were independently checked against linker maps in
both orders. Input-body matches use canonical relocation destinations:
raw REL addends can change during linking without changing the destination.
The diagnostic does not claim an original supplier or original input order.
Its manifest describes the supplied build, not historical provenance.

Eleven Gentoo-binutils controls include wrong immediates, reversed selection,
local-name collisions, moved relocation addends, incomplete manifests, and
an alien linked body matching neither input. Existing `byteident.py` and its
ratchet remain unchanged. Linked EXACT does not promote the conservative
EXACT set or establish whole-object identity or binding equivalence.

## Refusal triage before extending a table proof

[PR #335](https://github.com/philpem/slmodem_dsp_re/pull/335) inventories
76 native dispatch candidates after excluding the separately owned source
investigations. Fourteen have a same-size canonical counterpart; nine already
have an ordinary or induction proof. The five unproved pairs have independent
non-relocated byte differences:

| Function | Byte differences | Remaining obligation exposed by inspection |
| --- | ---: | --- |
| `v22_local_loop` | 4 | Private stack record passed to a callee, then read locally. |
| `RD_create` | 11 | Taken-edge guard and a separate escaped-frame obligation. |
| `VPcmFloModem::v90RunDemodulator` | 131 | More than one indirect control transfer. |
| `fax_class1_command` | 49 | Unsupported ABI data-relocation fields. |
| `voice_command` | 18 | Unsupported fixed-frame stack access. |

A wider proof domain alone cannot turn these pairs into EXACT. A first
refusal is also not a complete diagnosis: accepting `v22_local_loop`'s
initial SETcc would leave the later escaped record unproved. Keep the byte
comparison, table proof, frame proof and source diagnosis separate.

The retained extension has a different native witness:
`_Z19interpretMohTimeouts` compares AX unsigned against 13, takes JA to the
default, zero-extends AX into EAX, then dispatches through 14 ordered slots.
The zero-extension erases the upper bits that the comparison did not test.
The complete protected corridor must exclude bypass entries. Missing,
signed or mismatched extensions, changed flags, unsupported encodings and
out-of-domain bounds refuse.

Both real tables now prove, but their ordered identities remain different
and the function remains SIZE 34. Rebased fixture tables remain EXACT;
reordered or duplicated slots report RELOC. Refused fixtures remain
UNRESOLVED even when compared with themselves. No existential pairing of
case rows is introduced.

The extension passes 16 positive / 26 adversarial ELF controls and the
existing 3 positive / 32 adversarial ABI controls, all assembled by Gentoo
binutils. It also passes the existing relocation and ratchet checks. Full
census JSON and exact membership equal the frozen baseline.

## A bounded next lever: callee effects on an escaped local record

The new native witness for a follow-up is `V22_status`, range
`0x8c450..0x8c586`. Inspection covers all 94 decoded instructions. It has no
calls. ECX acquires the second argument at `0x8c456` and is not reassigned;
all 17 explicit non-stack memory stores use that base and fixed offsets.
Their byte union is `[0,12)` plus `[16,22)`. The stores at offsets 20 and 21
read/modify/write their bytes; the gap `[12,16)` is untouched. This agrees
with the independently modelled `struct v22_status` member layout.

At the caller, `0x8a938..0x8a94f` supplies frame `+0x20` as that argument.
The resulting writes lie in frame `[0x20,0x2c)` and `[0x30,0x36)`, below
the saved-register region `[0x4c,0x5c)`. The consumer at `0x8a98e` tests bit
0 at frame `+0x35`, exactly the output byte at `status+0x15`.

This is a bounded native write-footprint witness, not an implemented generic
callee-summary proof or a behavioural test. A sound extension would bind a
summary to the actual callee identity, establish its entry/return ABI and
non-escape properties, match the caller's actual argument, preserve partial
write masks, and prove frame separation. Controls must reject an unexpected
call, pointer-base mutation, write outside the record, or alias with saved
state. The current prover must continue to refuse until that integration and
its controls exist. `RingDetector_Create`'s frame-local input is another
candidate, with a larger transitive-callee obligation; it is not certified
by this leaf witness.

## Frontend cost is not fixed by the native helper

The installed `cc1plus` parameter table contains 35 records. Fresh decoding
confirms defaults 100 and 500 for `max-inline-insns-auto` and
`max-inline-insns-single`, respectively. Compiler SHA256:
`b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d`.
Lever 19's previous 130/600 values did not describe this compiler.

A two-cell full-TU replay compares the direct parent of #333 (`5862982a4`)
with its retained source at `60804df5b`, using the same current headers,
retained `.build-config`, and diagnostic dumps without parameter overrides:

| Source | `inverseMap` frontend cost | Helper | `process` |
| --- | ---: | --- | --- |
| Initialized scalar declarations | 98 | 440 bytes, EXACT | 458 bytes, SIZE 218 |
| Separate declarations and assignments | 102 | Same canonical body, EXACT | 240 bytes, EXACT |

All eight function names are preserved; only `process` changes. The retained
diagnostic build reproduces every production function body. The earlier
six-cell replay at `4a6519cba` recorded a 503-byte process; that is a dated
different-source measurement, not the result of this fresh parent replay.
Keep those baselines distinct.

This confirms #333's mechanism without restarting the separately owned
inline-edge screen. An exact helper body does not fix the original frontend
representation, inline cost or keyword. Nor does matching native operation
text distinguish a source inline call from copied source by itself.

## Validation and continuation

The Playbook review narrows Levers 0, 1, 14, 19 and 26, preserves their dated
measurements, and removes a duplicated statement-order paragraph. Equal
buckets, an exhausted finite family, absent cross-TU calls and final repeated
blocks no longer imply global source closure. Historical mutation records do
not create a mandatory maintenance step.

This pass does not resume the inline-edge screen, the Progress 88-instruction
gap, or the four separately owned arrangement re-audits. No source gate is
claimed for a new source candidate: none was retained here. The two apparatus
PRs and this documentation change leave reconstruction source, headers,
production flags and existing exactness policy untouched. Strict partial-link
semantic/raw completion remains a separate, unsatisfied project gate.
