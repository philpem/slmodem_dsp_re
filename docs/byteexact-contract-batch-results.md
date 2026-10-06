# Callback ownership and arithmetic operand batch

PR274 was merged at e0052eec before this batch started. Its retained Gentoo
baseline is1067/1852 exact functions and115564 exact original bytes. This
isolated branch avoids PR263's structure/V34/shared-gate scope; issue22 stays
read-only. Every experiment records the actual complete compiler command,
source/header/object hashes, initial RTL and later dumps, GCC3.4.2-r2 and the
selected assembler2.15.92.0.2. The bug define is appended last.

## Two adopted complete functions

* FAXVMI_process: EXACT327, by acquiring vmi->framer after processing callbacks,
  where the original reads it at0x95523. The five other TU bodies are unchanged.
* voice_modem: EXACT338, by writing det_len + saved for the final count. The
  original's heterogeneous MOVZWL/MOV inputs and ADD ownership predict this
  two-spelling control. Widths, declarations and callback interfaces stay fixed;
  the sum is bounded to131070 before its existing narrowing. The other eight
  TU bodies are unchanged.

Final production census1069/1852,116229 exact original bytes: +2functions,
+665bytes, zero losses. All300objects reviewed:298raw unchanged; two raw-equal
to independent winners. Configuration, records/binding/visibility/imports/
exports, allocated nontext, BSS and symbolic nontext relocations stay equal.
This is complete-body canonical byte identity, not alpha-renamed operands or
matching size. No source adoption from partial or negative controls.

## Bounded domains and their limits

| Area | Valid full-TU cells | Emitted body comparisons | Gains |
| --- | ---: | ---: | ---: |
| Wrapper unsigned arithmetic/fragment publication | 7 | 21 | 0 |
| FSE configuration-copy boundary | 4 | 16 | 0 |
| Tone reversal energy update | 2 | 22 | 0 |
| Fax owner/accumulator/cleanup boundaries | 26 | 174 | 1 |
| Service/data capture/cursor/count boundaries | 38 | 402 | 1 |
| GenericIIR/Sine entry boundaries | 15 | 302 | 0 |
| Total | 92 | 937 | 2 |

There are859shared-reference grades:78emitted C++ helper bodies lack a blob
counterpart and remain in the emitted-body audit denominator. The aggregate
auditor recomputes every live canonical grade and verifies saved source/object
hashes; family auditors check raw baselines, complete metadata and positive
detector controls. Six source-unchanged V21 caller bodies change in negative
controls through inlining; their source chunks and direct-call target multisets
are explicitly checked. HDLC unframe's negative control reorders exactly two
existing merge strings within an unchanged197-byte pool; full string multiset,
flags/alignment, names/data/BSS and symbolic data relocations are checked.
Neither collateral change is part of the adopted production delta.

Wrapper unsigned minima/remainders and post-copy fragment reloads reproduce
individual original operations, but do not recover the complete run function.
The fragment header overlay is unadopted and supplies no layout/API change.
FSE builtin memcpy at the original reset-store boundary prevents selected
hoisting; all four458-byte bodies remain nonexact. Separate tone energy
updates reproduce ADD-then-SUB but leave a SIZE36 miss. No further adjacent
operand/store/type permutations are justified by these outcomes.

GenericIIR member publication/ordinary inline helper visibility and signed
cursor controls explain individual missing operations but do not close scalar
or block bodies. Sine's common-entry loop also misses. A proposed broader
Sine type matrix was rejected before execution for lacking an independent
source discriminator; it is excluded from every count and left no executable
generator. These are finite negative source domains, not proof that original
source recovery is universally exhausted.

## Replay and next phase

Run the individual tools with their corresponding domain documents and a
baseline object tree built at e0052eec. The services mixed-sum screen accepts
--baseline-json and --objects; its known voice input fires in33TUs/26small
nonexact functions. Its six-instruction pattern finds no other candidate in
that scope, not a claim about all possible sums in the object.

Run tools/contract_wrapper_audit.py, fax_publication_batch_audit.py,
services_return_audit.py, batch_cpp_iir_audit.py and
batch_cpp_sine_entry_audit.py for family checks, then
tools/byteexact_contract_batch_audit.py for live grades and production proof.
Full objects/dumps/ledgers are retained under build; generated artifacts are
not committed. Use the ordinary tc/period build and strict1852-function census.

Next, hold the witnessed FSE copy graph fixed and inspect sched2 dependencies.
The started next-phase trace (tools/fse_copy_stage_trace.py) validates29RTL
stages: copy UID19 precedes freq UID29 and phase_acc UID45 through bbro,
including local/global allocation, reload and register renaming. sched2 first
reorders them to freq,phase_acc,copy, and later dumps preserve this order.
The duplicate-stream GCSE dump is explicitly excluded, not silently parsed.
This measures the changing pass, not the original scheduler cause. Separately, hold the V21
owner-reload model fixed and trace switch HI/SI conversion and inline caller
copies before allocation. Either investigation must predict a new source
boundary before another compile matrix. Keep profile work on22 with its owner,
and structure reconstruction on263 with its owner. Do not reopen the closed
AGC-return, formal-width, ordinary-inline or local-type families for score.

The requested five-to-twenty gains were not reached. Two gains are retained
because they have complete evidence; the negative measurements direct the
next phase toward pass tracing rather than more near-size variants. No fuzzing
or mutation harness ran; static anchors are checked by the final phase gate.


Final make tc and make phase both exit0. Period differential388passed/0failed;
all structural checks pass. Static anchors285suites/10038unique, no detached
or nonunique anchors. Newly staged references and tool syntax are rechecked
separately after documentation integration; no modern portability claim.

Staged reference check14382references/2931finding headings,0unresolved/held/
stale; static anchors remain clean. All24new Python tools parse. Family audits
and final live production audit pass. No generated output is committed.
