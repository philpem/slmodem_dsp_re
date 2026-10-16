# C++ full-TU source graph pass

Base240481e6, Gentoo GCC3.4.2-r2, retained flags and reproduction define. Four raw baseline objects reproduce byte for byte. `python3 tools/batch_cpp_audit.py` audits16cells,207emitted-body comparisons with symbol binding/visibility/imports/exports, allocated data, BSS and nontext relocation invariants. Positive control identifies BitsToSymbol reset change1/1; all other10bystanders unchanged.

## Complete gain

V90BitsToSymbol::reset(V90MappingParams*,PcmType), original108B vs retained149B. Direct if/else field assignments duplicate the subsequent clears and epilogue. Original initializes a zero accumulator before denominator test and joins one result store. Both ordinary conditional-expression and guarded unsigned local result reproduce EXACT108B; the two complete candidate objects are raw-identical. Adopt readable conditional expression, preserving the unsigned numerator, zero guard, mapper call and subsequent stores. TU10/11→11/11, one exact gain and no losses. InitialRTL extraSymbols member references2→1 while unsigned division remains1→1: common store factoring is established before scheduling, rather than dead-register fitting.

## Closed controls, no production changes

- V90Phase3Modulator updateCodeSegmentPointer: four helper/member×for/do controls all raw-inert28body TUs, original59B vs retained75B. Do not infer author do-loop from absent binary entry jump.
- V92Precoder reset: baseline plus scalar retained-pair/original-first-pair/grouped-copy controls recover232B vs123B loop baseline, but retain94/92/70different bytes respectively. No complete gain. Named fields and nontext remain unchanged; loop absence establishes expansion family but does not establish source spelling.
- V90SdDetector process: baseline144B vs original180B; pointer-tail167B, capturedresult166B, crossed173B, pointer/commonresult169B. Original reverse cursors and branch to result1 are recovered but no complete gain; no size-only adoption or threshold/NaN changes.

Domains and generators: `batch-cpp-{segment,precoder,sd,bitreset}-domain.md`, `tools/batch_cpp_*_reproduce.py`. Full commands, assembler execution identity, source/header/object hashes, initial/final RTL and changed body disassemblies are in corresponding build/batch-cpp-* packages. Final source needs parent combined period/structural gate before commit. No fuzzing/mutation runtime executed.

After this batch's shared fax header adoption, use --historical-headers with
the pinned240481e6 baseline objects/config when replaying these domains; the
shared generator otherwise correctly rejects header drift.
