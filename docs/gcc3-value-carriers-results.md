# GCC3 value-carrier results

Baseline3dbb1c33 on master after merging PR261 and PR262. This pass adopts
three source-backed byte-exact functions,303 reference bytes:

| Function | Reference bytes | Source discriminator |
| --- | ---: | --- |
| V90AutoDigitalImpDetector::updateLinMappMeanAndVar |128|Double addition mode for the half-offset|
| V90AutoDigitalImpDetector::updateLinMappMeanAndVarAlt |118|Double addition mode for the half-offset|
| ParallelDifferentialDecoder<unsigned char>::process |57|Separate narrow decoded result for two-address XOR|

Full production verification: both changed objects equal their independently
audited winners raw;298/300 other objects unchanged. Complete profile remains
Gentoo3.4.2-r2 with the recorded assembler2.15.92.0.2 and mandatory bug define.
No compiler options, comparator or symbol-binding changes. Final strict worst-copy census1042→1045/1852 and112866→113169 exact
reference bytes, zero losses. `make phase J=4` exits0:388 period
differential tests passed/0failed and structural gates green. Static
anchors285 suites/10038 rows,0 detached/nonunique; no mutations executed.

## The precision lever transfers; pool bytes alone do not decide the type

Two initial ADID controls establish the alternate update. The four-cell cross
then independently changes half-offset mode in both mean updates. Each alone
is exact; both coexist. Initial RTL changes the single addition SF→DF.
Original FLDS half before conversion and FADDP after calculation are restored;
constant payloads remain unchanged. The ordinary update also changes its
inlined copy in setQcLinearMapping, which remains SIZE16. This is an accounted
consumer, not an unexplained bystander or additional gain. All other TU bodies,
metadata, constants and canonical data destinations remain unchanged.

Shrinking ordinary code shifts seven .rodata table addends by16 bytes because
the unchanged studyUrefHandler moves. Each is proved to name the same unique
function and same relative instruction boundary. Data bytes outside relocated
fields remain exact. Audit normalization is local to these experimental data
checks; the production comparator is unchanged. An interior-instruction
address and a changed constant are demonstrated refusal controls.

## The decoder is a working-result identity, not a byte-versus-word XOR

The old cached-state/cursor control remains necessary and is not repeated as
an experiment. Retained and candidate initial RTL both already use xor:QI.
The baseline has an unnamed destination and state-first XOR operand; the
candidate has a named T-valued result updated with XOR. At20.combine the
baseline memory operand is first; the candidate input carrier is first and
state memory second. At26.postreload baseline loads state into EAX, whereas
the candidate retains a register-copy plus memory-source XOR, exactly as in
the original. The distinction starts before register allocation. An unsigned
input-width control was proposed but not executed because the initial trace
refuted a mode-width explanation.

Compiler dependency discovery covers all300 reconstruction inputs (299C/C++
and one assembly file), finding15 actual DiffCoder.h consumers. All15 raw
baselines reproduce. Across30 complete-TU baseline/candidate controls only
V90SignBitsExtractor's decoder body changes;14 candidate objects remain raw
identical. Headers/types/ABI, output-before-state writes, input/state/output
cursors and mutable size member reads are retained. The old historical18
consumer count belongs to another revision/scope; it is not reused here.
The first discovery assertion omitted the assembly source and refused before
compilation; corrected complete discovery is the only accepted inventory.

## Rejected F7846 family narrowed by a new discriminator

F7846's twelve float-mode spelling controls concern updateUref, not either
standalone mean method. Its original half load/pop witness justified a fresh
four-cell cross of double-half mode with the independently exact mean pair.
Uref240B changes BYTES86→BYTES10,79 instructions in both object and candidate:
all instructions/register operands now agree except frame allocation/release
and their derived stack offsets. The candidate reserves0x14 rather than0x10.
No literal/source padding or declaration fitting is attempted. The Uref change
is declined, although it narrows the residual substantially.

The control also changes the inlined copy in studyUrefHandler. Four of its
seven data-table targets move within that changed owner; instruction boundaries
and unique ownership are checked, and exact old/new destinations are retained
in the audit ledger rather than reported unchanged. No source from this
negative family is adopted. updateUrefAlt lacks the corresponding original
FADDP discriminator and is not attempted. VPcmV34GetCurrentTxBitRate is another
screened half-offset reserve with additional ownership differences, not a
measured candidate in this pass.

## Audit, replay and next discriminator

42 valid driver cells:2 initial ADID,4 ADID mean cross,2 decoder trace,
30 header consumers,4 Uref cross. These include explicit repeated baselines
and repeated source states; they are not42 independent source hypotheses.
Full-object audits account for962 strict body verdicts and26 RTL controls,
with no exact losses. Allocated data, exports, metadata and every changed
consumer are reviewed; function positions and non-adopted table deltas are
recorded separately. Two existing ADID semantic anchors are retargeted without
executing mutation suites. No runtime fuzzing or mutation execution.

On baseline3dbb1c33, archive300 `make tc` objects plus .build-config in
build/production-before. Each reproducer pins historical source; use
--historical-headers after applying the header change or on a later checkout.

```
python3 tools/gcc3_value_carriers_adid.py --domain docs/gcc3-value-carriers-domain.md --baseline-dir build/production-before --historical-headers
python3 tools/gcc3_value_carriers_adid_cross.py --domain docs/gcc3-value-carriers-domain.md --baseline-dir build/production-before --historical-headers
python3 tools/gcc3_value_carriers_decoder.py --domain docs/gcc3-value-carriers-domain.md --baseline-dir build/production-before --historical-headers
python3 tools/gcc3_value_carriers_consumers.py --discover --domain docs/gcc3-value-carriers-domain.md --baseline-dir build/production-before
python3 tools/gcc3_value_carriers_consumers.py --domain docs/gcc3-value-carriers-domain.md --baseline-dir build/production-before --historical-headers
python3 tools/gcc3_value_carriers_uref.py --domain docs/gcc3-value-carriers-domain.md --baseline-dir build/production-before --historical-headers
python3 tools/gcc3_value_carriers_audit.py
```

For consumer discovery, run on the baseline checkout: -MM deliberately measures
its actual header dependency graph. The remaining source replay uses an
explicit historical-header snapshot. ADID restricted dumps emit01.rtl,
20.combine and33.sched2; no ADID post-reload evidence is claimed. Decoder
full-da also supplies25.greg,26.postreload and35.mach. Known ADID full-da ICE
is avoided. Tools record complete commands/config/hashes and raw baseline
checks. Full artifacts: build/gcc3-value-carriers-{adid,adid-cross,decoder,
consumers,uref}, plus audit.json and controls/.

Next: trace which stack slot creates Uref's extra four bytes using its valid
initial/reload dumps and installed compiler allocation diagnostics. An actual
slot/lifetime witness is required before reopening a source domain. For the
new XOR lever, screen small two-address operations whose original preserves
input for a later store and computes a separate narrow result; don't infer an
original statement order from register colours alone. Issue22 remains read-only.
