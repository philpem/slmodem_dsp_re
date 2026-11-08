# TxBitRate recovery and Astra escalation, 8 November 2026

**VPcmV34GetCurrentTxBitRate is EXACT at 213 bytes**, from SIZE4/current217.
The complete TU retains every previous EXACT/REGALLOC copy. Minimum power stays
BYTES29 in the retained orientation and BYTES45 in its alternate orientation;
no additional neighbour promotion is claimed.

Baseline is `ba97e0eca3a3920c04fdf0a7d4ae67057f2dc0ad`. The selected source
combines a shared integer result, PCM-first source dispatch, the PCM switch,
direct fallback-member consumption and wider addition. It also includes the
object-neutral minimum-power member cleanup and corrected source comments.
The owner now requires Astra escalation after unsuccessful High and Extra High
investigations; AGENTS and the Playbook record that rule.

Final formatted source SHA256:
`a5d0477e5a60874ecffbbcf6beafb95bc74570911335c05540246e67c7c1620e`.
Its object is `747a3467d475366d9d3a32ee18a88bb9f4fbd6080942b0b041e61fdab0741dde`,
literally matching Astra's unformatted selected object. The
[evidence ledger](evidence/txrate-recovery-20261108.json) records native, source,
header, compiler/helper and parsed-object identities. Complete artifacts are in
`/mnt/zfs/dev/slmodem/scratch/txrate-wave-20261108`; parent gates are in the
isolated checkout's `build/txrate-wave`.

## Native algorithm and unchanged contracts

Native captures root+0x3548 session before the guards. Root+0x359c is a short
role and root+0 is full-word status. For PCM role 0x66, status 2 follows session+
0x6124 to the V92 modulator; status 3 returns 30000. Other roles use status 1/2
and the V90 modulator at session+0x1758. Both modulators require state/phase 3
at +0x2c before accessing the count chain; otherwise the getter returns zero.

V90 follows modulator+0x40, mapper+0, count+4; V92 follows modulator+0x4c,
transmitter+0, count+4. Counts are consumed unsigned. Multiplication by 8000
wraps as unsigned 32-bit before conversion; reciprocals remain float 1/6 or 1/12.
The native FILDLL uses a zero high word. FMULS then applies the float reciprocal;
FLDS/FADDP adds 0.5 at wider precision. The existing
`V90Demodulator::getBitRate` and F8060 provide this product-only widening
control. A float pool entry does not mean every operator has float precision;
whole-expression widening changes the multiply and is a different hypothesis.

The unsigned conversion uses FISTPLL under a temporary truncation control word,
returns the low word in EAX and restores the caller's control word. The shared
fallback reads signed root+0xaa88 and multiplies by 2400. Source preserves this
signed fallback and both unsigned PCM conversions. No public declaration,
object layout, flag, compiler profile or classifier is changed.

Native and selected have all 63 decoded instructions in the same positions,
with matching operands, flags, stack adjustment, branches and returns. The
getter contains no CALLs. Its three relocation fields have the same sites,
types, widths and canonical float identities: 1/6, 0.5, 1/12. This is complete
function instruction/relocation equality, not a bounded clean graph subset.
It does not recover unique source syntax or unique original declarations.

## Why High and Extra High did not close it

Sol High scores 44 full-TU cells: direct fallback, operand order, shared floating/
integer-result forms, wider literals and the existing product-only widening,
each crossed with both minimum-power orientations. None promotes. Commuting
0.5f is neutral; correct wider addition alone leaves SIZE6 with cfg retention
or SIZE9 with direct fallback. The existing return graph still keeps session
in EBX and preserves that register, unlike native EDX/ECX ownership.

Sol Extra High scores 76 cells and observes a distinct cause: ce2 hoists the
K56 result, creating status/result interference. A PCM status switch removes
that overlap and the callee-save, reaching native extent. Case order, source-
role identity and widened-sharing controls still fail strict comparison.
There are 81 compilations including baseline/dump controls; all 41 protected
copies survive. These are bounded negatives, not an impossibility claim.

High and Extra High used independent frozen baselines and parsed compiler
stages. Their reports retain hypotheses, complete-TU effects and ordinary/dump
parity. The source constraints transfer into the Astra escalation rather than
being repeated as a generic spelling search.

## Astra's decisive cross

Astra adds shared integer result × PCM switch × outer source-role order.
The first controls reach BYTES2: all native structure agrees except the fallback
MOVSWL/IMUL input register. The matched cross keeps direct fallback, widened
addition, case order and ready-state ternaries fixed:

| Result structure | Non-PCM source arm first | PCM source arm first |
| --- | --- | --- |
| Direct returns | BYTES135 | BYTES135 |
| Shared integer result | BYTES2 | **EXACT213** |

All four repeat in the alternate minimum-power orientation. Neither isolated
source-property result justified rejecting the coupled source. The selected
switch places default last; the equivalent middle-default control is exact too,
so one unique case spelling is not asserted.

Compiler dumps locate two effects. Shared result changes the K56 equality edge
probability from 210/10000 to 2900/10000 at bp, leading BBRO to place K56 before
V92. The last two bytes change at rnreg: non-PCM-first renames fallback EAX to
ECX; PCM-first reverses earlier chain traversal and retains EAX. The fallback
remains BB11. BBRO still emits native non-PCM-first machine order despite the
PCM-first source. This is observed register/branch-layout causality, not a
general claim that function order or incoming TU state is irrelevant.

There are 54 audited candidate cells/54 source hashes/22 object hashes,
58 compilations including baseline and three dump controls. The audit retains
all 56 functions, 103 exports, 41 protected copies, 225 CALL identities and 81 data
continuations per cell. Fourteen cells reorder the same three float-pool words;
raw pool changes and canonical relocation identities are recorded separately.
No artificial carrier, padding, weakened grade, tolerance or profile change
is retained. Actual token/billing usage is unavailable; this one successful
escalation does not establish model-wide cost efficiency.

## Minimum-power Astra review and byte-neutral cleanup

The separate Astra review completes 24 scored TUs/23 source hashes/12 object
hashes plus three dump controls. It gains no EXACT/REGALLOC function. It checks
the scope of old negatives before crossing target-local beta lifetime/PHI and
native SetTxScale value-source controls; the latter are raw-neutral.

One actual-member control is retained: initializing short want from the declared
int `cfg->unnamed_0044` preserves native signed low-word consumption and emits
the same complete object in both minimum-power orientations. It removes an
incompatible short-pointer dereference without changing the member type or
inventing a declaration. The obsolete raw-access macro is removed; the header
now documents explicit narrowing. These cleanups remain object-neutral when
crossed with the exact TxBitRate source. The parent reformats only whitespace
and updates the header comment without changing its declaration or line count.

Minimum power's earlier conditional cursor negatives remain tied to their
particular source/search graph. This wave does not transfer them to an untested
graph or claim a new minimum-power recovery merely because an earlier getter
has become exact.

## Fixture domain and parent validation

No maintained zero-only fixture is added. A scratch survey covers 16 native
Create/modeminit histories, 32 getter calls and 2,016 raw region comparisons;
all retain status 0, ratecfg.txbits 0 and return 0. Public SetV90 does not establish
PCM readiness. Ten period layout assertions pass. The initial survey's wrong
session-1 role expectation is preserved as an invalid apparatus run, then
corrected against native construction without touching reconstruction source.

Existing t_v34pcmif probes cover V90/V92 arithmetic, ready guards and signed
fallback, including unsigned-product wrap boundaries and UINT_MAX. These remain
synthetic component probes; no negotiated-ready protocol history is established.
The fixture audit verifies 533 frozen inputs, 300 raw/testhost payloads and 307
linked inputs per focused run. Baseline t_vpcmcreate passes 40,659 checks/19 groups
and t_v34pcmif 63,365/38 groups. This qualified coverage is separate from the
complete native equality of the getter.

The parent independently rebuilds unchanged and selected full TUs. All previous
EXACT/REGALLOC copies are retained and only TxBitRate changes machine body.
The source-level minimum-power cleanup and header documentation are measured
separately. `make phase J=8` passes 399 period tests, zero failures, and the
structural checks. The 300-object closure changes only Main. The baseline and
selected strict partial links both remain DIFFERENT. The graph screen selects
17 inexact copies, comparing 245 owner relations with 30 ambiguous anchors;
its bounded clean result does not establish equivalence. The evidence ledger
records the complete denominators and refusals. The global census is 1,160 EXACT,
33 REGALLOC, 79 BYTES and 580 SIZE across 1,852 symbols under this baseline
classifier; integration with remote master requires fresh validation.

## Next steps

Transfer the learned return-graph/source-arm/lifetime mechanism only with a
fresh native witness, and check existing helpers before introducing a new
arithmetic spelling. Minimum power remains parked with its documented native
false-beta liveness/alias/helper/search-graph reopening conditions after all
three model efforts; do not restart its closed palettes.

First reconcile local master with origin/master in the isolated integration
worktree, preserving both branches' recoveries and comparison-tool repairs.
Rerun the period gate and classify the merged corpus using the integrated
classifier before nominating the next function. The remote branch contains
independent source and proof changes, so this wave's baseline census cannot
serve as the merged result. Whole-object raw identity and valid PCM-data
lifecycle coverage remain open objectives.
