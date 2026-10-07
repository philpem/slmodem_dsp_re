# Callback value ages and shared tails: continuation of PR280

Work continues on investigate/x87-scheduling at9025b8d8, retaining its EIA6
gain. No shared headers, profiles, PR263 paths or issue22 writes are changed.
All experiments retain Gentoo3.4.2-r2, its executed selected assembler and
reproduction define last; complete commands/hashes remain in build ledgers.

## Three complete gains

**V17TX_control,148B.** Original request scale is loaded once, first written
to PPS, multiplied from that stored value, then published only after the
intervening owner+18 assignment. Source re-read request, used a derived PPS
address, published scale too early and duplicated return-one after reinit.
Eight crossed scale-value/whole-owner/common-return cells recover148B BYTES8.
Original three-instruction order then motivates four capture/publication
controls. Only delayed publication is strict EXACT. Both original scale
stores and unknown alias-visible value ages are preserved.

**V27TX_control,148B.** Original independently has the same first scale store
and delayed multiplied publication, which retained source omitted. Its flag
byte is read again after the conditional source+8 assignment; retained source
kept a stale snapshot. Four crossed cells: only scale graph plus fresh flag
uses is EXACT. Original V27TX_status remains exact. No assumption that request,
source and DSP owner cannot overlap is introduced.

**fax_class1_status,150B.** Original has an early zero result, two ordered
state groups4..6/12..13, shared status callback and one epilogue. The shared
result alone still emits subtract/range predicates and two tails. Cross the
witnessed ordinary grouped switch: only switch plus common result is EXACT.
Default0/eligible1 and template initialization/callbacks remain preserved.

Full production objects repeat all three strict controls. Of300 production
objects, only these three TUs change; no bystander bodies, allocated data,
bindings/imports/exports/BSS/nontext relocations change. Two control/status
bodies are reviewed in each transmitter TU,17 bodies in class1.

## Closed controls and remaining witnesses

**selectFilter:** eight crossed callback pointer ages, earlier type1/type2
carriers and four switch trees give0 gains/losses, preserving EIA6. Only the
target body changes. Switch controls reorder the merge-string pool; the full
string multiset and section extent agree, all other data unchanged. Do not
adopt the closest SIZE control. Original register/operand/SETcc and clamp
graphs remain unresolved after this finite family.

**V29TX_control:** four scale/flag-age controls and four captured-byte
verdict-expression controls miss strict identity. The latter crosses ordinary
if, boolean and conditional forms; do not continue adjacent spellings or
register choices. Its original captured byte across member publication remains
real evidence, but no production change is adopted.

**V17/V29TX_status:** six cells each cross final input-byte capture before the
adjacent flag clear with direct/local/two-bit first clears. All miss. No
bitfield overlays, alias coercion or type changes are used. Their original
register read/AND/store versus memory AND remains a separate source/codegen
question; a mask spelling is not uniquely established.

**TxHdxEQCondV27:**21 complete-TU cells,210 live body grades,0 gains/losses.
Unsigned budget, actual member subtraction, per-iteration rate reads and
pointer/postdecrement loop are crossed, then independently observed pre-call
cursor/count lifetimes tested. First CSE removes the extra countdown read,
while branch rate reads survive. These actual source-fidelity leads are not
discarded as mere register renaming: high-bit budgets and overlapping output/
rate histories can matter. No source adoption on a partial body. See
[full bounded evidence](fax-eqcond-reload-results.md).

Small C repeated-return screen:335 nonexact bodies with original<=850B,
five extra-epilogue/callback candidates. Two close above; pulse readiness and
tone detection have existing closed families and are not blindly replayed.
Receive-side sibling review examines8 functions in4 TUs (3 already exact),
with no fresh common-return discriminator:
existing exact siblings stay fixed, and rejected snapshot/flag domains require
independent changed evidence to reopen.

**bValidateEnergyValue:** four crossed shared-success/sequenced-RMS controls,
24 live body grades,0 gains/losses. Actual initial RTL recovers history-index
read after the RMS call. Original FDSP_Kernel_InitObj call remains inlined
in every candidate. Original/current effective ABI both pass buf/n in EAX/EDX;
the source's old external/ordinary-convention comment is stale. No ABI/profile
rewrite or partial source adoption. See
[energy callback controls](energy-callback-result-results.md).

## Audits and constructor explanation

Root callback audit:50 full-TU cells,302 live body grades,10 raw baselines,
3 strict gains/0 losses. Positive detector observes all three complete wins.
No unexpected bystanders; only explicitly audited negative-control string
pool reorder. External EQ-conditioning audit adds21 cells/210 live grades; constructor
forensics adds8 cells/72 grades, energy adds4 cells/24 grades. Combined
83 valid full-TU cells/608 live body grades, plus the bounded RX review.

The red historical constructor membership floor was valid when recorded.
Eight forensic controls/72 body grades pinpoint the loss at2efa968b's correct
+434 int→float recovery, which changes only C2's ECX/EDX roles: BYTES4.
The writer itself and all data/metadata remain unchanged. Dump-only pre/post
replays preserve2 raw objects/18 bodies and examine116 clone streams; C2 first
diverges at28.peephole2 scratch selection, then30.rnreg gives final colors.
See [the stage proof](v90-parameters-constructor-stage-proof.md). Stock/Gentoo and
math-profile A/B controls refute those alternative explanations. Keep the
float recovery and the floor. See
[constructor provenance and replay](v90-parameters-constructor-ratchet-history.md).

Final production gate: make phase exits0,388 passed/0 failed, structural
checks green. Strict exact-name census1071→1074/1852, original exact bytes
117641→118087; exactly the three functions above gained,0 lost. Relative to
master before PR280, the branch has four gains including EIA6. No fuzzing or
mutation harness execution, no modern portability claim. Historical constructor
floor remains red for its already identified collateral loss.
Next transfer requires actual value ages, publication order, dispatch operands
or tail edges; duplicated source returns alone do not establish a recoverable
graph, and partial matches never establish a global byte-exactness ceiling.

Final static refs exits0:14384 references/2958 finding headings,285 suites/
10038 static anchors;0 unresolved/held/stale/detached/nonunique/no-op/wrong-arm.
Nineteen new or changed Python tools parse. Named state constants repeat the
unchanged complete production class1 object. No runtime gate was broadened.
