# Count ownership and terminal sample narrowing

Baseline: master14769433, after #266. Three further strict function gains are
retained under the unchanged Gentoo GCC3.4.2-r2 profile:

| Function | Reference bytes | Recovered source property |
|---|---:|---|
| `GenericIIR<float,double>` C1 constructor |107| Each coefficient count owns its derived buffer length before coefficient-pointer stores |
| `V92Phase3Modulator::generateJa` |77| Unsigned wide sample magnitude, one terminal signed-short conversion |
| `V92Phase4Modulator::generateE1u` |54| Unsigned wide sample magnitude, one terminal signed-short conversion |

The combined census is1049→1052/1852,113629→113867 exact reference bytes,
**three gains/238B, zero exact losses**. This compares function bodies with
typed canonical relocations; it does not establish positioned partial-link
identity. Five unresolved symbols remain unchanged. Production changes exactly
three of300 objects; each equals its independently audited winner raw and
297 other objects remain unchanged. Complete `.build-config` is unchanged.
No headers, layouts, profile options or definition order changed. The other
session's structure inventory and issue22 remain separate.

## The constructor: source ownership changes local allocation

The blob stores denominator count and derived output length before numerator
count/derived input length, then the borrowed coefficient pointers. Retained
source previously stored pointers first and both counts before both lengths.
Three full-TU controls distinguish count/length grouping from pointer-store age:

| Cell | Exact/common | `blockSize` lreg span | Allocated hard register |
|---|---:|---:|---|
| Baseline |8/12|16 instructions|4, ESI|
| Count/length pairs, pointers later |9/12|12 instructions|2, ECX|
| Same pairs, pointers still early |8/12|16 instructions|4, ESI|

Installed compiler dumps identify `blockSize` as pseudo63 in both constructor
copies. Local allocation (`24.lreg`) already chooses ESI in both losing cells
and ECX in the winner. The final winner removes saved ESI and reproduces all
107 bytes, including the original8-byte allocation, allocation calls and
reset sibling jump. The retained version saves ESI/EBX and allocates4 bytes;
those numbers alone would misclassify it as an alignment-option problem.
This experiment does not trace a new callee-publication change. It demonstrates
source live-range ownership before local allocation, distinct from #265's
known-callee alignment and #266's post-call vector indexing.

All18 emitted functions are checked in each cell, including six absent from
the reference comparison. Only the C1/C2 constructor copies change; only C1
is a new reference exact. Symbol metadata, imports, binding, allocated data,
BSS and canonical nontext relocations are unchanged. Neighboring FloatIIR
and FloatARMA constructors are already exact; no blind transfer is made.

## Wide magnitude is distinct from a short local

Ja's original `MOVZWL`, full-width `NEG` and one terminal `MOVSWL` predict
`unsigned int sample = (unsigned short)codeLevel`, with `(short)` at return.
The field and exported return type retain their types. Four controls cross
that property with explicit level capture immediately after the scrambler
call. Width alone recovers77B; the explicit capture factor is raw-inert both
with and without it. Twenty bystanders, including TRN's five RTL stages,
remain identical. The inlined Ja copies shorten the enclosing pump1453→1443B
(reference1437), still nonexact. Its16 switch destinations retain their
decoded owner/instruction-boundary identities with recorded offsets.

E1u has the same original magnitude/narrowing witness. The four independent
CPt/E1u controls produce only the54B E1u gain. CPt's new wide carrier reaches
BYTES24 from BYTES29, but does not recover its original register ownership;
it is declined. F8141's96 cells tested signed/unsigned *short* carriers, and
the subsequent amplitude cast domain kept short locals. Neither exhausted
the unsigned32-bit magnitude with terminal conversion tested here. Reopening
that bounded question does not reopen declaration/XOR-order permutations.

E1u's inline arm supplies a stronger control than whole-pump size. Its old
negative path copies EDX→EAX (2B), negates EAX and sign-extends AX→EDX (3B).
The winner directly negates EDX, matching the reference, and uses the existing
final DX→EBX signed conversion. The five-byte reduction makes that state arm
88B, the original extent. Its amplitude load also becomes original `MOVZWL`.
Threshold-load scheduling and scratch registers still differ. Whole-pump
size moves4008→4003 versus4055 reference: the SIZE gap grows47→52 despite
recovering this local mechanism. Do not restore redundant narrowing to fit
an aggregate size. Only E1u and its inline pump change;42 bystanders and
all symbol/data controls remain identical. Every table displacement is retained
in the audit at a decoded instruction boundary inside the same pump.

For every16-bit amplitude representation, both bit outcomes retain the signed
low16 result, including0x8000. Unsigned wide negation expresses the magnitude
without intermediate signed-short narrowing; no sign claim about the member
or new overflow behavior is introduced.

## Coarse opcode screening needs value and CFG tracing

The new read-only screen examines all300 objects/1886 common defining copies,
with a declared300B maximum reference body. Eleven copies contain a call and
later `MOVZWL`, `NEG`, `MOVSWL`; four are already exact and seven are nonexact.
The real E1u case fires; an instruction-list control without NEG is refused.
The screen is deliberately not a same-value/path proof.

The seven candidates separate as follows:

- Ja and E1u are the genuine magnitude/narrowing gains above.
- `v8_ansamgenerate` negates the final amplitude store, unrelated to the
  sample narrowing. Prior counter/cursor/phase domains stay closed.
- `RxHdxDataV17/V27/V29` use NEG to form a quality boolean mask, then AND a
  demodulated count and narrow its return. Six controls change result ownership
  to unsigned32 with terminal short conversion, holding calls, flags, output
  stores and quality conditions fixed. No gains/losses: V17 staysSIZE2,
  V27 movesSIZE3→SIZE1, V29 staysSIZE6. Current lowering still branches rather
  than reproducing the original SETNE/NEG/AND. No near-hit adoption or flag cross.
- `twoLevelDemod` negates before its decoder calls, so it is not that magnitude
  witness. It has a separate original pointer reload after `isAltRbs`, absent
  from the cached-local source. Two source controls recover that reload but
  remain nonexact (SIZE5→BYTES79 at the original200B extent); all25 bystanders
  and data remain identical. No adoption or register-fitting follows.

The two large V90/V92 pumps receive a separate four-cell ownership cross.
Explicit wide level capture/late sample narrowing and direct member indexing
canonicalize to the retained post-call instruction ranges. Isolated cells
remain18/28 exact; combined17/28 loses exact JdNot. No adoption. The combined
bystanders JdNot/Scrambler C1 first differ in35.mach; initial, combine, greg
and postreload agree. Remaining local/remote negative tails, zero epilogue
sharing and inner Sd dispatch layout are real CFG placement differences;
addresses do not identify a unique source statement boundary.

Thirty saved baseline dumps make the next discriminator concrete, without
another compilation. The zero-result termination arm already shares its return
in01.rtl (UID1360 event store,1364 sample-zero,1366→label1411→return chain).
Hardware epilogue instructions first appear in27.flow2. The zeroing peephole
first appears in28.peephole2, while the final return-label replacement first
appears in34.stack; that saved-stage boundary alone does not identify which
internal routine made the replacement. The inner Sd dispatch retains its
GTU guard UID98→label86 and adjacent dispatch/table through30.rnreg.
In31.bbro the guard becomes LEU to newlabel1887 and the table moves behind
the timeout blocks. Its REG_BR_PROB remains5000. Thus the remote Sd table
is a measured compiler block-placement effect. The original separate zero
epilogue still does not distinguish a source early return from compiler return
duplication. Keep those two conclusions separate rather than asserting that
all layout differences originate in the same pass.

## Replay and validation

The23 valid complete-TU cells use the full recorded period flags with bug
reproduction appended last and execute the compiler-selected assembler.
All eight unchanged TU controls reproduce their archived baselines raw.
They supply516 strict common-body verdicts and534 emitted-body comparisons.
No fuzzing or mutation harness is run. An initial P4 locator failed before
any compilation; a premature production inspection saw an incomplete tc
build and failed its raw-winner assertion. Both are excluded, retained as
invalid artifacts, and repeated after the correct boundary.

Seed `build/production-before` with all300 #266 production objects and their
`.build-config`. Drivers pin14769433 sources; use `--historical-headers` if
replaying after header changes. From this worktree:

```sh
python3 tools/gcc3_iir_constructor_reproduce.py --domain 'three count/length ownership cells' --baseline-dir build/production-before
python3 tools/gcc3_v92_sample_reproduce.py --domain 'four Ja width/ownership cells' --baseline-dir build/production-before
python3 tools/gcc3_p4_sample_reproduce.py --domain 'four independent CPt/E1u wide sample cells' --baseline-dir build/production-before
python3 tools/gcc3_sample_ownership_reproduce.py --domain 'four V90/V92 pump ownership cells' --baseline-dir build/production-before
python3 tools/gcc3_sample_count_reproduce.py --domain 'six masked-count ownership cells' --baseline-dir build/production-before
python3 tools/gcc3_demod_member_reproduce.py --domain 'two post-isAltRbs pointer-owner cells' --baseline-dir build/production-before
python3 tools/gcc3_sample_transfer_audit.py
python3 tools/gcc3_v92_sample_reproduce.py --audit
python3 tools/gcc3_sample_count_reproduce.py --audit
python3 tools/gcc3_sample_tail_screen.py
python3 tools/gcc3_pump_stage_trace.py
```

Drivers retain source overlays, commands, compiler/assembler identities,
hashes, assembly, RTL, complete verdicts and audit ledgers under the matching
`build/gcc3-*` directories. Inline E1u tracing and pump negative analyses are
also preserved there. Final production census/raw-object ledger and the
period/structural gate are `build/sample-byteident.json`,
`build/sample-production-audit.json` and `build/sample-phase-final.log`.

The initial gate passed388 period tests/0 failures but refused six detached
anchors. Their find/replace text was retargeted to the same bit/sign operations;
labels, suite membership and counts are unchanged. The static check now reports
285 suites/10038 anchors, zero detached/nonunique/mispointed. The final complete
`make phase J=4` rerun exits0:388 period tests passed/0failed and all
structural checks green for the composed source and updated provenance.
