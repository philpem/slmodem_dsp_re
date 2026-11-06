# V34 owner recovery and complete flow comparison, 6 November 2026

`VPcmV34SetMinimumSigLevel` is now **EXACT104**. The full census advances from
1,147 EXACT / 29 REGALLOC / 72 BYTES / 604 SIZE to **1,148 / 29 / 71 / 604**
over 1,852 functions, with no EXACT/REGALLOC losses. InitMOH remains BYTES55;
Retrain improves SIZE51→SIZE11 but is still incomplete. Those partial results
are not counted as recovered functions. The
[evidence ledger](evidence/flow-retrain-recovery-20261106.json) records inputs,
controls, closure and validation.

## What produces the exact helper

Native retains root+4 for the control prefix. In the minimum-level helper,
the unsigned debug test at 0x6391 precedes the floor store at 0x639f, addressed
as engine+0x22c
(root+0x230). The diagnostic rereads config+0x60 at 0x63b8 after publication;
count clears at engine+0x230 (root+0x234). Indexing still uses unsigned
level+0x30, rejects >7 and defaults to entry 3.

Raw integer access supplies the interior address but keeps the debug read
after publication. Flat typed access supplies the early comparison but selects
the root base. A real embedded `v34_engine_control` supplies both and recovers
all 104 bytes. A source boolean snapshot instead emits SETA/TEST and fails to
recover the native graph; it is not retained.

The prefix covers existing root+4..0x263 fields. A GNU anonymous union exposes
a named POD prefix and the flat compatibility view, with identical fields
from one guardless header. Both records have the same complete common initial
sequence and only trivial scalar/array members. Actual period C89/C++98 probes
accept the model and assignment/read compatibility. Permanent assertions check
all 29 fields' root and prefix offsets and widths, prefix size 0x260 and root
size 0xac4c; receiver remains at root+0x264. The field file uses `.h`, so both
period newest-header invalidation and object dependency tracking include it.

This is an inferred ownership/alias model, not a recovered original type name
or proof that the original author used this union. Typed uses cover SetMinMax,
SetMinimumSigLevel, Retrain and HangUp. Every replaced use retains its complete
owner, offset and width. SetMinMax and HangUp remain byte-exact.

## Retrain: shared algorithms and remaining constraints

Native 0x6667 retains the engine address across rate, signal-level, delay,
switch and handshake work. Its 0x68dd progress publication follows handshake
0x68d8. The existing public rate/floor/delay and reset helper algorithms match
the repeated native regions; the source now shares their implementations.
Native session recaptures at 0x667e and 0x674d distinguish the helper boundaries
from an entry session retained for later sensitive-ISP/session-type work.

| Source experiment | Retrain result |
| --- | --- |
| e815 baseline | SIZE51, 1,355 bytes |
| A: complete retained engine owner | SIZE35 |
| B: reset helper alone | Literal unchanged |
| A+B | Same as A |
| Rate helper sharing on A+B | SIZE51 |
| Delay helper sharing on A+B | SIZE10 |
| Rate+delay helpers on A+B | SIZE26 |
| Floor helper/owner plus native diagnostic reread | SIZE5, 1,411 bytes |
| Final typed embedded model and shared helpers | SIZE11, 1,395 bytes |

Native extent is 1,406 bytes. These controls retain worsening intermediate
states; isolated grade changes do not disqualify their supported source
properties. The final embedded model changes the alias contract and must be
judged as its own combination.

A separate complete CFG comparison pairs 64 reachable blocks, 97 typed edges,
35 conditional guards and 14 calls. It found no extra jump or guard explaining
the residual. The raw 1,321,920 acyclic syntactic paths are not all value-feasible
and were not exhaustively symbolically executed. Seven ordinary follow-up
controls isolate an extra request conversion: sharing the first switch's int
conversion with its V90 diagnostic removes the extra MOVZX while preserving
the independent default diagnostic reload. Together with independent DC-owner
groups and the native V90 capture, that diagnostic reaches 1,406 bytes and
305 reachable instructions but remains BYTES225. It is not adopted as a win.
Debug sampling, entry-request liveness and publication schedules still differ;
matching CFG topology does not establish an allocation-only residual.

## Collateral: investigated and retained

Both complete 300-source closures compile; all 300 baseline objects reproduce
e815 literally. The parent independently rebuilds the final source and gets
300/300 literal agreement with the frozen candidate closure. Six objects change:
V34RX, V34TX, V34hshak, v34scram, v34shell and VPcmV34Main. Across these objects,
145 baseline/candidate functions pair, 121 also have native counterparts and
16 bodies change. All 43 protected EXACT/REGALLOC functions remain protected.

| Inexact collateral | Baseline | Final | First executable compiler change |
| --- | --- | --- | --- |
| SetDelays | BYTES116 | SIZE26 | gcse debug PRE/HOIST |
| Create | SIZE105 | SIZE121 | gcse debug transport/reuse |
| ApplyBulkDelay | SIZE39 | SIZE46 | gcse debug PRE/HOIST |
| descrambleGPC | SIZE5 | SIZE21 | postreload reload after rx_data store |

Flat prefix references become conservative alias set 0; named prefix accesses
retain component metadata. Raw RTL metadata changes at initial RTL, before the
listed executable transformations. Seven ordinary/dump controls establish
literal parity before attributing these effects. Delays' native retained base
is root ESI, with session EDI captured at 0x63ff. Its dmadelay stores use
root+0x25c, not engine+0x22c. No invented interior-base repair was added.
These inexact regressions are carried with the supported exact recovery and
remain concrete follow-up targets.

Exports, bindings, emitted function order and allocated non-code layouts and
relocation-zeroed payloads agree throughout all 300 objects. Canonical call
sequences agree in the six changed objects. Complete relocation tables differ:
V34hshak has 2,347→2,346 records and main 528→529, with one fewer/additional
debug-global reference respectively. All 403 ordered code-data targets retain
owner-relative identity and decode at instruction boundaries: 314 in handshake,
89 in main. Raw addends move at 215 and eight sites respectively. These facts
are not a claim of exhaustive switch-value equivalence.

Routine owner screening compares 277 relations through 285 matched anchors,
with 105 bounded/16 refused functions, 69 ambiguous anchors and zero nominations
in both endpoints; unpaired anchors increase 48→50. Stack screening compares
six relations through 29 anchors; bounded/refused changes 22/99→21/100 because
Delays leaves its supported prologue model. Zero nominations from these subsets
are not equivalence evidence for all 121 functions.

## Complete InitMOH caller-flow audit

The [optional closed-case tool](../tools/native_graph/initmoh_flow_case/README.md)
now goes beyond the earlier bounded graph screen. Against the actual final
object, it covers all 97 decoded instructions, 95 reachable instructions,
11 reachable blocks, 13 reachable edges and nine feasible normal-return paths.
The two unreachable instructions are identical padding. There are no caller
loops or indirect branches. The instruction map is manually reviewed for this
function shape; it must be reviewed again if the shape changes.

All 720 dynamic instruction pairs, 21 call boundaries and nine returns match
in the model. All 3,603 obligations discharge: 3,585 identical expressions and
18 UNSAT difference queries, with zero UNKNOWN. Independent SAT witnesses cover
18/18 complete native/candidate path domains. Path domains are proved equal
before instruction comparisons, rather than restricting evidence to their
intersection. Late and flag bytes are independently symbolic over 0..255.

The proof tracks register byte lanes, flags, stack cells and whole modeled
memory. The late-byte value and constant-five value have separate lifetimes;
there is no single global EAX/ECX substitution. Independent receiver/reset
stores reorder but have equal memory at the tone call and return boundaries.
This adds no REGALLOC/EXACT promotion and identifies no unique original local
declaration or allocator constraint.

The model assumes normal returns, valid addresses and the documented i386 ABI.
Root, config and session may alias one another, but their ranges exclude the
fixed debug/string globals and private stack. Opaque callees receive equal
entry targets, arguments, memory and frames; they may then write arbitrary
matched external memory and caller-saved state. Callee-saved registers/private
stack preservation are explicit contracts, not proofs of callee internals.
This is caller equivalence within that model, not public negotiation coverage.

Parent review reproduced two false cleans in an early auditor: changing a
PC32 CALL addend -4→0 reaches entry+4, and changing a global addend 0→4 reads
the next word. Symbol-only canonicalization lost both differences. The final
reader validates exact relocation type/site/addend, rejects overlapping fields
and alternate entries, and retains full diagnostic string bytes. Another
control reproduced solver AST-ID reuse, which corrupted serialized DAG node
provenance despite unaffected live solver expressions. Strong AST references
fix that artifact defect. Controls must reject for the intended typed reason;
an unrelated exception is a failed control.

The runner freezes ELF/script bytes, runs the audit, independent path witnesses,
and normal/optimized calibration, and propagates all exits. It installs nothing;
Z3/Capstone/pyelftools are optional and outside the normal phase gate. Metadata
and runtime dependency versions are recorded separately because the installed
Capstone distribution says 5.0.9 while its module reports 5.0.7.

## Fixture scope and gates

The older Retrain fixture has 22,645 checks across nine synthetic behavior
groups and one hole group. Its patterned internal states are explicitly labeled
synthetic. New coverage uses registered id34 factory output and exported
constructors for both sides: 90 Retrain cases, 270 calls and 3,157 checks, plus
54 direct-helper cases, 108 calls and 759 checks. Debug levels 0/1/2 and requests
0/34/56/90/92 are crossed. Direct helpers compare all 126 regions/265,992 bytes
without alias masks, immediately after the call; all 27 configured output-change
observations fire.

Constructed Retrain stays at status 0: the generic V34 rate arm is exercised;
designer/K56 arms and accepted PCM session requests are not. No unsupported
session permission flags were planted. Root/config/session are separately
allocated and debug hooks do not reenter, so this domain does not establish
config/root alias histories or mutable session-pointer recapture behavior.
Native instructions independently support those source boundaries.

Retrain installs ten root alias words/40 bytes. Twelve other previously learned
bytes, including untouched timing pointers, are compared raw. Existing saved-
snapshot comparator calibrations remain. The expanded old endpoint passes
12,368 t_vpcmcreate checks before source adoption. An initial helper expectation
incorrectly required generic rate-helper debug output; native prints none on
that branch. Only the expectation was corrected, and the failed apparatus run
is preserved.

The first phase run passed 399 period tests but failed a leftover mandatory
legacy source-spelling check in `refcheck.py`. Its old K56Flex pattern mistook
the correct typed clear for a live mutant. Retired suite/anchor reads are removed
from the citation gate, completing the documented retirement; no mutation suite
or anchor was repaired. A citation-gate control shows valid citations pass and
dangling citations still fail without opening legacy registries.

Final `make phase J=8` passes 399 period tests, zero failures, and all structural
checks. Final-object tool controls pass 21/21 ELF cases, 512/512 expression
lifetime samples and 28/28 intended-refusal checks in both normal and optimized
Python; independent domains are 18/18 SAT with 21/21 call preconditions admitted.
Citation controls pass 6/6 in both modes. No modern portability verdict or
billing/token-efficiency claim is made. Source/fixture
investigation agents used Sol High; difficult flow/alias reasoning used Sol
Extra High. These successive tasks were not a blind model comparison.

## Next discriminating work

1. Resolve Delays' root/session/config value ages around both debug checks and
   echo-delay publication. Trace its native alias contract against the gcse
   substitution before proposing a coupled header/source repair.
2. Compare Retrain's minimum-level and low-count debug sampling boundaries with
   caller publications and entry-request liveness. Use a full lane/value-flow
   audit, not only CFG isomorphism or native-sized output.
3. Transfer the typed interior-owner lesson to other native root+4 consumers
   only where their address and component metadata are independently witnessed.
   For InitMOH, a new allocation constraint is needed; another type/store-order
   grid or a clean bounded screen does not supply it.
