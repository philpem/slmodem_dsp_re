# Callee-alignment exploration after PR265

Base2001434c, merged PR265. The other session owns structure inventory/PR263;
this branch changes no header, field, layout, visibility or optimization flag.
Issue22 read-only. No mutation/fuzz execution.

## Three exact gains from the near-frame screen

| Method | Reference bytes | Retained | Adopted |
|---|---:|---|---|
| V90Phase3Modulator::generateJdPhase |80|SIZE2|EXACT|
| V90Phase3Modulator::generateV92Jd |80|SIZE2|EXACT|
| V90Phase3Modulator::generateTRN1d |60|BYTES2|EXACT|

Both vector methods previously pass their member array to vectorBit before
quotient work, leaving an extra saved register. Original integer quotient
work precedes the vector-base load. Direct member indexing recovers this
ownership and the original one-register frame. Post-call original ownership
then separates the two methods: JdPhase updates/tests polarity before loading
the signed return level, while V92Jd loads the zero-extended level first.
V92Jd uses a wide unsigned level, optional wide negation, then one short
conversion at return. Narrow updated controls add a conversion after NEG;
wide signed control differs in the initial MOVSWL versus original MOVZWL.
All negative boundaries are retained; no field-type change is inferred.

The independent four-cell final cross is essential. V92-only gains its80B
but loses exact generateJdNot. JdPhase-only gains80B and TRN60B but loses that
same bystander. Together all three gain and generateJdNot remains exact.
Only the combined winner is adopted. No definition/source-order permutation
is used to obtain the incidental TRN gain.

The real caller observations distinguish this from Uref's mechanism. Across
all four cells, known Scrambler process info has boundary0 and the caller's
preferred boundary remains128bits. There are no local bytes: all16 observed
slot requests are zero-size BLK reload alignment requests.196 frame-layout
calculations replay the actual formula. Retained two saved registers mean
return4+saved8+outgoing8=20, padded to32, requiring20 allocated bytes.
One saved register means4+4+8=16, requiring8 allocated bytes and no padding.
It is saved-register pressure plus fixed call alignment, not a lowered callee
requirement. Raw driver/plain/GDB assemblies and reassembled objects agree in
all four observational controls.

TRN source is unchanged. Its initial/combine/global/reload RTL agree after
removing only compiler pointer addresses, assembly-label IDs and source lines;
35.mach first differs. Reused installed peep2 observer records four target
searches with identical eligibility: entry cursor3 selects AX, while entry2
selects CX; later machine output is DX versus original CX. All272/272 actual
SI/general searches replay under the captured eligibility/order, with unsupported
mode and corrupt-selection refusal controls. This is compilation history, not
another source recovery in TRN or evidence of changed incoming alignment.
The observer now accepts cc1plus and an isolated output path; old cc1 defaults
and1accept/5refuse completion controls are retained.

## What the wider screen says

All300 objects yield1886 common defining copies. The diagnostic stack-erased
screen finds1070 normalized instruction streams, zero remaining strict or
colour-erased stack-only candidates; its known F11796 Uref control fires and
a non-stack instruction change is rejected. Four near candidates under800B
and16 differing diagnostic rows are triage only. B103's row is additional
same-size epilogues/CFG, not a different allocation; GenericIIR's constructor
also differs in stores/reset inlining. They do not supply a callee-alignment
source preimage. The two vector candidates lead to the gains above. No
production comparator erases frames, colours or branch identities.

The69-TU C++ DSP/V90 call-boundary inventory contains910 common copies,
633 exact copies and31 differing call rows. Its only two extra library-call
rows are the already closed nanf sites. A bounded PreFilter four-cell terminal
return cross recovers its original one CALL/two tails, but179B still misses
original180B; all16 bodies retain6 exact, and15 bystanders are unchanged.
It remains unadopted. The ordinary case3 call means no reduced alignment
transfer is predicted. Full-TU/data/import/control proofs accompany this
negative. See [inventory](gcc3-alignment-callee-inventory.md).

## Reproduction of the alignment publication rule

Eleven two-function micro-TUs include strong leaf, external-call wrapper,
ordinary/aligned double slots, weak leaf and explicit template leaf. All11
have raw driver/plain/GDB assembly and object replay, with five source-order
pairs raw-identical. Installed binds_local returns and cgraph availability
are observed without inferior calls or writes.

Strong leaf publishes32bits; an external-call wrapper publishes128bits.
Even an explicitly aligned8-byte local still publishes32bits in this profile:
local alignment is not an incoming call requirement. An emitted weak/template
leaf publishes0, leaving its caller at128bits. GCC's publication is guarded
by targetm.binds_local_p; weak/linkonce binding can block publication even
when asm_written is true. No diagnostic attributes/binding/order are copied
into reconstruction source. Source-order invariance is measured only for
the stated five pairs, not asserted universally. [Micro controls](gcc3-alignment-micro.md)
link the official sources and installed-binary results.

## Audits, gate and replay

Twenty vector driver cells include repeated baselines and follow-up states;
560 strict body verdicts and five TRN pass controls. All complete-TU symbol
metadata, named/allocated nontext data, BSS and canonical code/data relocation
identities are identical. Only the two vector bodies and unchanged-source
TRN change in the final winner;25 other bodies remain raw-identical. Four
PreFilter cells add64 strict verdicts, all nontext unchanged, two historical
self-UNRESOLVED anonymous selectors explicitly preserved.11 micro-TUs are
compiler apparatus, not source comparisons to the blob.

First archive all300 production objects and .build-config at2001434c into
build/production-before. Source reproducers pin that revision; for later
checkouts use --historical-headers. Docker, pyelftools, native32-bit execution
and GDB Python are required for observer replays. Complete compiler and
executed assembler versions, commands and hashes are saved in build artifacts.

```sh
python3 tools/gcc3_alignment_stack_screen.py --positive-control PATH_TO_F11796_DOUBLE_HALF_OBJECT
python3 tools/gcc3_alignment_callee_inventory.py
python3 tools/gcc3_alignment_callee_inventory.py --prefilter-reproduce --baseline-dir build/production-before
python3 tools/gcc3_alignment_callee_inventory.py --prefilter-audit
python3 tools/gcc3_alignment_micro.py
python3 tools/gcc3_alignment_vector_reproduce.py --baseline-dir build/production-before
python3 tools/gcc3_alignment_vector_narrow.py --baseline-dir build/production-before
python3 tools/gcc3_alignment_vector_late.py --baseline-dir build/production-before
python3 tools/gcc3_alignment_vector_split.py --baseline-dir build/production-before
python3 tools/gcc3_alignment_vector_consumer.py --baseline-dir build/production-before
python3 tools/gcc3_alignment_vector_observe.py
python3 tools/gcc3_alignment_vector_audit.py
python3 tools/gcc3_mechanism_scratch_observe.py --compiler-kind cc1plus --reproduction-dir build/gcc3-alignment-vector-split --target generateTRN1d --out build/gcc3-alignment-scratch
python3 tools/gcc3_alignment_scratch_model.py
```

The stack-screen positive is the actual archived Uref double-half-only object
from [the previous four-cell reproduction](gcc3-uref-stack-results.md); its
--positive-control argument makes that fixture dependency explicit. Do not
substitute an arbitrary object or trust an empty control. The screen only
ranks candidates and does not certify semantics or exactness.

## Next discriminator

For a frame residual measure local slots, saved registers, outgoing arguments,
padding and actual callee publication/binding separately. Search original and
reconstructed library-call differences in both directions, including original
calls versus reconstructed open coding. A weak callee with published0 cannot
supply a smaller caller requirement without a separate source/binding witness;
do not force hidden visibility or function order. Near-frame source ownership
can recover pressure even when the known-callee path is unavailable. Broader
score-only spellings are not justified by this bounded screen.

## Final all-object call census and integration

The bidirectional extension covers all300 objects (299 C/C++ plus pow.S),
1886 common defining copies/1075 exact copies,84 differing call rows and11
CALL/tail-only substitutions. Extra-library occurrences under the explicit
allowlist are memmove1, nanf2, sin1, cos1 and fmodf1; original-only library
calls are zero, including CALL-plus-tail accounting. These are emitted-copy
counts, not the strict worst-copy unique denominator. Math sites retain their
closed profile/header provenance domains. RcFixed has blob0/ours1 memmove,
not an inverse original-library-call lead.

RcFixed needs separate caution: its source explicitly declines the original
kind-1 mode0/1 multistage conversion arm, documented in F11450 and in the
RcFixed_Resample guard. Therefore2640/493 is not established to be solely an
inline-budget or alignment difference. Current modem callers use modes2..7;
that reachability limitation neither reconstructs the omitted object arm nor
explains its bytes. Before a future copy-loop control, isolate the original
polyphase compaction region from that separate arm and measure count/direction/
overlap. This pass does not change the API, structures or declined branch. The census
counts decoded named direct rel32 relocation edges; unresolved/ambiguous
controls are explicit rather than inferred as calls. Production has zero such
unsupported controls and zero equal-range alias edges. ELF alias-positive and
unequal-overlap negative controls demonstrate the ownership detector.

Final production:1046→1049/1852 strict exact;113409→113629 reference bytes,
three gains220B, zero exact losses. The one changed V90Phase3Modulator object
equals the audited combined winner raw;299/300 other objects and .build-config
unchanged. make phase J=4 exits0,388 period differential tests passed/0failed,
structural checks green.285 suites/10038 static mutation rows remain unique;
no mutation/fuzz execution. Playbook/findings updated. No claim of a global
byte-exact ceiling or uniquely recovered author spelling.

To replay the all-object inventory:

```sh
python3 tools/gcc3_alignment_callee_inventory.py --all-tus --output build/gcc3-alignment-callee-inventory-all.json
```
