# GCC3 mechanism pass results

Baseline9f1199b5 (PR261), complete Gentoo3.4.2-r2 configuration unchanged.
The pass gains **V92Modem::reset,138 reference bytes**: strict worst-copy
1041→1042/1852 and112728→112866 exact reference bytes, zero exact losses.
The only production change is its illegal-side diagnostic arm's break→return.
The rebuilt V92Modem object equals the independently audited winner raw;
299/300 other rebuilt objects equal the archived baseline raw. Final
`make phase J=4` exits0:388 period differential tests passed,0failed,
structural checks green. No runtime fuzzing or mutation execution.

## What now bounds the remaining work

[The full immutable inventory](gcc3-mechanism-classification.md) accounts for
811 baseline nonexact symbols/819 defining copies over300 objects. Only40
have a complete existing alpha proof with established relocation identity.
673 differ in instruction sequence;83 additionally classified by size and
typed direct-call target/count differences; the remaining classes account for
operand, destination-proof and dead-scratch differences. These are observed
categories, not exclusive causal diagnoses. Neither register allocation nor
inlining budget explains the entire remainder on this evidence.

[The terminal census](gcc3-mechanism-owner-screen-results.md) finds three
fresh candidate functions in91 C++/service objects (960 defining copies,
310 nonexact). Ten valid complete-TU cells give one gain. V90Modem::reset
and ADID::findPadGain remain nonexact; their bounded crosses are declined.
Five further paired ownership reviews find no missing input/member reload
boundary. This is not an exhaustive alias-aware ownership analysis.

[Four constellation-counter controls](gcc3-mechanism-constellation-domain.md)
recover original unsigned loop branches but no exact function. All four
complete-TU metadata/data/relocation/bystander audits pass. No source adoption.

Thus19 valid compiler-driver cells comprise10 terminal complete-TU cells,
4 constellation complete-TU cells and5 scratch-trace inputs. The last three scratch inputs deliberately
remove exported functions and are diagnostic only. The ten terminal cells
include two deliberate baseline/terminal repeats. Each scratch input also has
plain and observed host cc1 emissions, both raw checked against its driver.
Invalid tracing and prior
invalid classification artifacts remain excluded; they are not extra cells.

## A reproduced compiler mechanism, rather than a register spelling search

`peep2_find_free_register` uses the TU-wide static `search_ofs` described in
F7812. This pass observes the installed Gentoo cc1 at function entry and
candidate selection, without inferior calls or writes. It preserves both
unobserved assembly and the complete assembled driver object raw in all five
cells. The compiler binary hash, complete driver/assembler commands, function,
requested mode/class, exclusions, all53 allocation-order entries, eligibility
and cursor transitions are saved. The executed assembler is2.15.92.0.2.

For FDSP_DP_Run, baseline and the two-real-helper source form both enter the
last scratch search at cursor49 and select hard register1 (EDX), exiting2.
The live set at that search is EAX/ESP. The helper form reproduces137/138
reference bytes; the final deallocation POP remains EDX rather than ECX.

| Diagnostic input | Total scratch searches | FDSP entry cursor | Selected scratch |
| --- | ---: | ---: | --- |
| Complete retained baseline |53|49|EDX|
| Complete two-helper wrapper |53|49|EDX|
| Helpers and FDSP only |2|1|EDX|
| Helpers, FindCorrelation and FDSP |5|15|EDX|
| Complete helper TU minus FindCorrelation |50|3|EBX|

The last control was declared after the trace identified FindCorrelation as
the final predecessor: its three searches advance3→13→15→49. Removing that
whole predecessor leaves bSearchEnergy's exit3. The model predicted EBX,
and the unchanged compiler emitted EBX. Removing exports is not an adoption.

The finite transfer model reproduces154/163 observed choices in its declared
SImode/general-register domain; nine other searches are explicitly outside
that model. All163 observations remain recorded. Under the matching helper
cell's captured live set, class and allocation order, only entry cursor2 of53
selects the reference ECX. This is a conditional constraint on a candidate
original compilation history, not proof that the original used this profile
or that its TU entered at2. No compiler cursor was forced/reset.

Initial tracing using optimized DWARF parameter locations at raw entry was
invalid: arguments read garbage despite unchanged output. Its artifact is
preserved and excluded. The valid observer reads cdecl arguments from the
entry stack and asserts their domain. Host dump-file permission failures are
also apparatus failures; separate host-writable observation folders fix them.
Positive firing is demonstrated on five FDSP searches. Classifier controls
accept4/refuse7; trace completion controls accept1/refuse5 (including missing
target); model refuses unsupported mode and corrupted selection.

## Replay and artifacts

Create this pass on9f1199b5, build the complete baseline with `make tc`, and
archive its300 objects and `.build-config` in `build/production-before` before
adopting any source change. All reproduction tools pin historical source;
the baseline directory must come from that revision. The classification and
owner domains list their commands. Root scratch replay:

```
python3 tools/gcc3_mechanism_fdsp_reproduce.py \
  --domain docs/gcc3-mechanism-fdsp-domain.md \
  --baseline-dir build/production-before
python3 tools/gcc3_mechanism_fdsp_reduce.py
python3 tools/gcc3_mechanism_scratch_observe.py
python3 tools/gcc3_mechanism_scratch_observe.py \
  --reproduction-dir build/gcc3-mechanism-fdsp-reduced
python3 tools/gcc3_mechanism_scratch_observe.py --self-test
python3 tools/gcc3_mechanism_scratch_model.py
python3 tools/gcc3_mechanism_classify.py --self-test
```

Docker, a host capable of running the installed32-bit cc1, GDB with Python,
and nm are required. Unsupported compiler symbol/source locations refuse
rather than producing a clean empty report. The diagnostic line breakpoint
is specific to the recorded Gentoo compiler. Observe uses its saved driver
command; model reads both named results files, not the latest snapshot alone.
Full artifacts remain under build/gcc3-mechanism-{fdsp,fdsp-reduced,scratch},
including model.json and compiler SHA256. Final census is
build/gcc3-mechanism-final-byteident.json; gate log/exit are
build/gcc3-mechanism-phase.{log,exit}.

## Next discriminating work

1. Trace the40 complete-alpha cases as small TU/pass families, distinguishing
   pre-reload ownership from post-reload scratch and rename effects. A
   register-only proof supplies a classification, not the source spelling.
2. Check closed-family history, then trace the sparse MOV/MOVZBL carrier in
   ParallelDifferentialDecoder<unsigned char>::process and x87 operand width
   in updateLinMappMeanAndVarAlt through expansion/combine/scheduling.
3. For FDSP, require an independent source/predecessor witness that predicts
   the needed cursor transition before another source experiment. Do not
   permute function definitions or insert dummy allocations to obtain2.
4. Keep selector/data destination proofs separate. The four unresolved
   selectors and getSegmentPointer need ownership/extent proofs, not a
   relaxed comparator. Issue22 and the other session's trace queue remain
   outside this pass.
