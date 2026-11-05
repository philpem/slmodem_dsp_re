# Dialer source recovery, 5 November 2026

This wave recovers `AnalyseDialString` EXACT436 and `IsDialStringInvalid`
EXACT149. The complete six-function Dialer TU is audited; four bodies remain
inexact. Evidence is indexed in
[dialer-recovery-20261105.json](evidence/dialer-recovery-20261105.json).

## Source recovery and coupled repairs

| Function | Baseline | Selected | Native size |
| --- | --- | --- | ---: |
| AnalyseDialString | SIZE23 | EXACT | 436 |
| GetNextDigitAndReturnNextState | SIZE118 | SIZE1 | 895 |
| IsDialStringInvalid | SIZE14 | EXACT | 149 |
| DialerCreate | SIZE8 | BYTES5 | 269 |
| DialerProgress | SIZE445 | SIZE445 | 2431 |
| DialerAbort | SIZE5 | SIZE1 | 145 |

A grade suffix is the classifier's mismatch count, not a correctness metric.
The analyser's selected EXACT additionally requires the table-proof repair
below. Source-only and classifier-only census effects are reported separately
in the evidence ledger.
The census moves from 1,144 EXACT /28 REGALLOC /70 BYTES /610 SIZE to
1,146 EXACT /28 REGALLOC /71 BYTES /607 SIZE, over 1,852 functions. The new
proof alone changes no baseline grades. Both EXACT gains require source
recovery. No protected EXACT member is lost; the REGALLOC count stays 28.

Native `0x7af89..0x7af8c` consumes the analyser result with unsigned SETBE.
Native `0x7af6d..0x7af7e` prepares its name before the second debug-level guard.
A crossed experiment retains baseline, unsigned consumption alone (SIZE14),
preguard lookup alone (BYTES4), and both (BYTES2). The remaining bytes are real,
unrelocated local-call distances. Native function order is Analyse, GetNext,
Invalid, Create, Progress, Abort. Recovering that order and the predecessor
source graphs is necessary to close the wrapper; call-distance normalization
would conceal the dependency.

The producer's grade domain is 0..3. Its signed selector, early loop index,
direct unsigned grade-lowering predicates and nonzero validation arm recover
all 436 code bytes and all 88 ordered table targets. This does not establish a
negative-grade bug, an unsigned field declaration, or unique original syntax.
The local and stored grade remain int.

GetNext uses the native signed selector, four separately guarded constant A-D
row publications, nonzero-position-first mode arms and successful mixed-mode
arm ownership. A promising all-break control temporarily lost the wrapper's
EXACT grade. It remained active: crossing the space/default continuation graph
and mixed-mode arm repaired the wrapper and reached SIZE1. The native table
covers 32..119 with 88 entries; the former source table covered 33..119 with 87.
All retained source arms have native control-flow witnesses. No inert carrier,
volatile, flag or padding was introduced.

Create now prepares the grade name before the diagnostic guard, rejects using
unsigned consumption and reloads the modem owner independently for the two
pulse setter calls. Native owner reads at `0x7b030` and `0x7b045` support these
ages. Known ordinary setter histories do not replace that owner, so this is
source-observation recovery rather than a demonstrated callback defect.

Abort's normal-arm ownership restores the native unsigned JA and three
physical diagnostic sites after an intermediate tail-merging regression.
Its error diagnostic still tail-calls where native calls and returns. An
explicit-return control is object-identical; no unsupported operation follows
the call merely to suppress optimization.

## Detector repair and controls

The old table proof refused the recovered analyser despite matching code,
because EBX's outgoing argument was mistaken for another original ABI save
and an ordinary frame local was unsupported. The extension admits aligned
fullword unknown locals only with predispatch initialization and initialization
on every admitted path. It accepts exact register SETcc encodings and SBB;
existing guard, selector, slot, width and stack protections remain.

Review also found an old unsound acceptance: an entry jump can reach a higher
block that clobbers EBX, then jump backward to the first lexical save. Address
order cannot prove original register identity. The repaired CFG state tracks
which pending save identities remain pristine and rejects this case, including
partial-register clobbers. Locals remain unknown across calls; no value or
semantic-equivalence claim follows from frame safety alone.

`jumptable_fixed_local_controls.py` retains 134 positive cases and 55 refusals.
Both normal Python and `-O` pass using explicit checks. A rebased pair remains
EXACT; swapped ordered targets score RELOC. The real recovered analyser proves
88/88 targets. Actual parsed object and tool-byte hashes are retained. These
are detector controls, with no mutation-suite infrastructure involved.
Nine inherited proof suites also pass. One integration test initially crashes
on a refused overlapping owner because body() returns (None,None). The old
detector reproduces it; a minimal harness repair explicitly checks NODATA for
that sentinel. Repaired integration output agrees between old and new detectors.


## Whole-TU and fixture limits

All300 reconstructed objects were compared with the frozen baseline:299 are
literally unchanged and only Dialer changes. Its ordinary, dump, observed,
replayed and integrated objects agree at SHA256
`cc59d8f6e9d52a30ae01055a0f661a7198e230def6638b44bbe5a7fa7e5bf61b`.
All225 tracked header/compatibility inputs were frozen. Exports agree; the
emission order now agrees with native.

All187 code pointers reach instruction boundaries: Analyse88, GetNext88,
Progress11. All47 string payload/owner occurrences and the two frequency
arrays agree with baseline; the four grade names retain their ordered values.
Raw packed layout changes and rodata grows 780 to 784 bytes. Target ownership
for inexact functions does not establish target-offset or CFG equivalence.

Progress source and its2,876-byte body/instruction sequence remain unchanged;
its four nonrelocated differing bytes are local-call distances. The final
Abort correction changes unchanged GetNext assembly from 894 to 896 object
bytes. All its normalized RTL stages and its assembly block are identical
between those endpoints: assembler relaxation/alignment context must be kept
separate from source and allocator explanations.

The owner graph screen admits 2/6 pairs and refuses 4; the stack screen admits 1/6
and refuses 5. Refused indirect/tail-call graphs are not clean equivalence
results. The33 graph controls pass normally and with `-O`.

The fixture audit adds real overlength, NUL-terminated FATAL inputs to wrapper
and constructor diagnostics and constructor object/timing checks. The old and
selected sources both pass these expanded four focused suites; this is domain
coverage, not an old-fail regression claim. Ordinary timing defaults replace
an overflowing derived harness fallback. Legacy planted internal states remain
synthetic probes; these are component tests, not full modem lifecycle proof.

The full `make phase J=8` passes 399 period tests with 0 failures and all
structural checks. Citation checks are rerun after the documentation updates.

## Residuals and next work

Create's remaining constant-store scratch choices first appear at peephole2;
the native owners, widths, offsets and values are retained. Reopen with a
supported predecessor/source change that affects the scratch stream, not an
initializer permutation grid. Abort requires new continuation/type/profile
evidence before another return spelling experiment. GetNext requires native
case/return-sharing evidence; SIZE1 alone is not proof of its source graph.

Next, transfer signed selectors, predicate ownership and prediagnostic
observations to fresh inexact functions. Check the Playbook's closed controls
first. Retain promising temporary collateral losses and judge the repaired
combination with full TU effects and period fidelity.

The next concrete transfer is `chkForceBaudRate` (SIZE5) in
`VPcmV34Main.cpp`. Native captures `obj->p3548` at0xa479 before the diagnostic,
while reconstruction loads it afterward. Native cap branches at0xa4e7..0xa50b
are unsigned JBE/JA; source uses signed comparisons. Cross early session
capture with unsigned consumption, then inspect the `obj+4` owner view if
needed. The selector is an unsigned byte shifted five, domain0..7; no negative
selector bug is established. Audit `t_v34pcmif`'s construction boundary first.
