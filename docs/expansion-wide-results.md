# Widened expansion screen and branch-call controls

Base276d30b5, merged PR276:1070/1852 exact,116851 exact original bytes.
PR263's V34/structure work and issue22 writes are excluded. No production
source, headers, compiler flags, mutation metadata or stored verdicts change.

The declared [screen domain](expansion-wide-screen-domain.md) covers95 TUs,
997 emitted bodies and298 nonexact original functions at most4096B:
240 at1..800B and58 at801..4096B. Four indirect/unrelocated calls are excluded
explicitly. Canonical symbol calls nominate just two functions. The historical
rolled ConstellationPower positive fires: original/rolled modulo5/1 and
division6/2. The exact current helper is excluded from nominations. The fixture
is a real full period TU, SHA256
`3404b2abdb6a8bc21faf65c0249d55788441ddcf96c16d2a30dd086951a27a59`.

| Nomination | Original/retained bytes | Repeated call original/retained | Classification |
|---|---:|---|---|
| V90Phase3Modulator::reset |479/427|resetDILGenerator2/1|Mutually exclusive mode arms |
| V92Phase3Modulator::reset |351/289|edprintf2/1|Mutually exclusive length arms |

Neither supplies the previous fixed-entry expansion witness. Counting sites
does not distinguish source repetition from optimizer replication, nor does
it prove a source-recoverable mismatch.

## V92 length arms and warmup

Original16c27 and16cb3 print the same published trn1uLength after separate
minimum8160/calculated-length stores. Its unsigned threshold JA places the
minimum on fallthrough. The warmup zero-check, counter copy and DEC/JNE are an
independent descending-counter witness. Four full-TU controls cross explicit
length/diagnostic arms with unsigned descending warmup. Signed duration
arithmetic, unsigned cap including equality, one encoded diagnostic per
execution, Ja/debug/float operations and call count are preserved.

| Arms | Countdown | Bytes | Exact functions in complete TU |
|---|---|---:|---:|
| Shared, baseline | No |289|17/22|
| Shared | Yes |288|17/22|
| Split | No |341|17/22|
| Split | Yes |340|17/22|

The split cases retain two edprintf calls through global reload: UIDs93/117.
`27.flow2` leaves only117; `31.bbro` creates a second site (332 or330).
The baseline has one throughout. This supplies known firing merge and
duplication observations: late emitted call counts cannot identify the number
of source statements. All four candidates remain nonexact; closeness in size
does not authorize a new nearby spelling family.

The audit rechecks four live hashes/source hashes and88 body grades, raw
baseline identity, symbol binding/visibility/import/export records, allocated
data/BSS and every nontext relocation. Only reset changes in three controls;
all bystanders are unchanged. No source adoption follows.

## V90 mode arms preserve a shared dynamic warmup

Both original mode arms reset DIL (2c3ac/2c45d) then join the same loop2c46a.
The loop reloads sessionFlag2c47f each iteration; fixing the warmup mode is
refuted. A two-cell arm-local DIL control keeps that behavior. Two source calls
survive through postreload260/327, merge to327 at flow2 and stay shared.
Both complete objects are raw-identical,18/28exact, reset427/479B. Audit56live
body grades and58single RTL streams, all metadata/data/BSS/nontext relocations
and text positions; exclude cgraph and duplicate-snapshot GCSE dumps explicitly.
[Complete evidence and replay](batch-cpp-p3-reset-results.md).

Replay:

```sh
python3 tools/v92_reset_branch_reproduce.py --domain docs/v92-reset-branch-domain.md --baseline-dir build/production-before
python3 tools/v92_reset_branch_audit.py
```

Artifacts are under
build/v92-reset-branch; the audit refuses missing/duplicate RTL streams.

## Replay and next discriminator

Use retained production objects/config from276d30b5 as build/production-before
and its byteident JSON as build/baseline-byteident.json. Recover the positive
object from the pre276 production build at6b4509bd, or replay that revision's
complete ConstellationPower TU under its recorded config; verify the fixture
hash before interpreting the screen. Run tools/expansion_wide_screen.py with
`--objects`, `--baseline-json` and `--positive-object` pointing to these inputs.
Its JSON retains all298 eligible rows, excluded call denominators, nominations
and positive evidence. Generated object/dump inventories are not committed.

This closes the declared call-repetition widening, not every remaining
function. Larger bodies, indirect/section/interior calls and fully inlined
arithmetic are outside its evidence. The next expansion lead needs independent
fixed-index access/arithmetic evidence in one of those excluded classes;
another call-count or source-spelling sweep is not a new discriminator. A
separate pass can screen fully inlined fixed-index arithmetic, with an explicit
known positive and complete graph review before compilation. No blanket
inline-budget explanation or reconstruction ceiling follows here.

## Final checks

Six valid full-TU controls,144 live body grades,0gains/losses; retained
production1070/1852 unchanged. Five new Python tools parse and both full-TU
audits pass. Staged reference check:14382references,2941finding headings,
0unresolved/held/stale. No production change requires a new differential run;
no mutation/fuzz execution or modern portability claim.
