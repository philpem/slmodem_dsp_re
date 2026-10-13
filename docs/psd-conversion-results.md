# PSD scalar recovery and conversion attribution

Baseline5cd55f7a, unchanged Gentoo profile and mandatory bug define. This pass
recovers getFrequencies and bounds process's remaining six register rows.
It does not establish whole-object identity or recover original compiler state.

## Getter recovery

An unchanged read-only stack observation identifies the extra4B getter word as
SF expansion temporary through assign_386_stack_local/gen_truncxfsf2, not a
spill. Four complete-TU cells/52 body verdicts cross ordinary scalar arithmetic
and bins>i independently: baseline SIZE1, scalar-only BYTES23, comparison-only
SIZE1, both EXACT77B. Preserve (i*sampleRate)*reciprocal and final store narrowing.
No rate capture, guarded-do or reassociation controls were repeated.

All13 shared bodies are reviewed: only the getter changes; process and the other
11 bodies remain unchanged. Symbol metadata, named data and allocated nontext
sections are unchanged. Five saved/plain/observed raw-equal getter observations
report8 stack requests: scalar cells have zero locals and only a zero-size reload
request; baseline/comparison cells retain the expansion SF4 word. Natural scalar
mode plus original comparison ownership is a source-supported exact gain under
the fixed period profile, not a claim about modern rounding.

## Process attribution

Two unchanged complete-TU preparations preserve the production raw object. The
annotated -dAP scratch replay refuses assembly equality because RTL comments
contain process-specific addresses; preserve this refused replay and exclude it.
The unannotated replay preserves saved/plain/observed assembly and complete
object raw identity. It reports9 scratch events,5 in process (UIDs464,196,130,71,65),
none at the remaining conversion sequences. The scratch-push prediction is false.

Initial RTL zero-extends unsigned frames to DI (UIDs356/403), then converts to
float (357/404). At flow2, conversion low words already occupy AX and high-word
zeros DX. Peephole2 replaces high-word SETs788/781 with XOR forms831/828, retaining
DX. Register renaming then selects:

| Sequence | Chain UIDs | Incoming | Selected |
|---|---|---|---|
| Linear low | 787,790 | AX | SI |
| Linear high | 831,789 | DX | DX |
| Decibel low | 780,783 | AX | BX |
| Decibel high | 828,782 | DX | AX |

A read-only observer pinned to the actual installed cc1plus hash records44
process rename decisions, including these four chains, unavailable registers
and all53 use-age ticks. Assembly and complete object remain raw-equal. The blob
uses SI/BX and AX/DX respectively. All60 x87 rows and27 branch rows remain exact;
process635B/BYTES6 is accepted by the existing live-range alpha comparator.

[Official GCC3.4.2 regrename.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/regrename.c)
explains function-local zero-initialized ticks and strict smaller-tick selection.
Downloaded source SHA2560255019a3fbb26685db7d35e47a9e090a6e89727d4fe8934246ca92ae6d8bafb.
This stock source explains the mechanism; installed compiler DWARF/decisions
anchor the Gentoo observation. Unlike the persistent peephole scratch cursor,
these ticks reset at function entry. Changing an earlier function cannot directly
reset them. Original RTL/ticks are unavailable, so these observations do not
identify a unique original source spelling or justify arbitrary history changes.

## Replay

```
python3 tools/gcc3_psd_conversion_prepare.py --domain docs/psd-conversion-trace-domain.md
python3 tools/gcc3_psd_conversion_prepare.py --domain docs/psd-conversion-trace-domain.md --no-asm-annotations
python3 tools/gcc3_mechanism_scratch_observe.py --reproduction-dir build/gcc3-psd-conversion-plain --compiler-kind cc1plus --target process --out build/gcc3-psd-conversion-plain-observation
python3 tools/gcc3_psd_rename_observe.py
python3 tools/gcc3_uref_stack_observe.py --reproduction-dir build/gcc3-psd-conversion-plain --family psd --target getFrequencies --generic-trace --output-dir build/gcc3-psd-frequency-stack
python3 tools/gcc3_psd_frequency_scalar.py --domain docs/psd-frequency-scalar-domain.md
python3 tools/gcc3_uref_stack_observe.py --reproduction-dir build/gcc3-psd-frequency-scalar --family psd --target getFrequencies --generic-trace --output-dir build/gcc3-psd-frequency-scalar-stack
python3 tools/gcc3_psd_final_audit.py
```

Preparation wrappers pin pre-adoption revision5cd55f7a and require a raw baseline.
When replaying after adoption, use a separate baseline worktree or the shared
runner's --baseline-dir with its unchanged .build-config and archived canonical
object. The final audit requires integrated source equal to the chosen cell.
Artifacts retain commands, hashes, compiler provenance and raw observation controls.
No fuzzing/mutation execution, compiler-state writes or profile/comparator changes.

## Final integration

Chosen canonical source/object raw-equal the independent winning cell. Exactly
1/300 canonical objects changes;299 and complete .build-config remain unchanged.
Census1081→1082/1852 EXACT,119682→119759 original exact bytes; SIZE672→671,
REGALLOC31/BYTES63/UNRESOLVED5 unchanged. No baseline exact losses, ratchet green.
make phase J=8 exits0:388 period differential passed/0 failed;285 suites/10038
static anchors attached, unique and correct-arm. None executed. Reference checks
against5cd55f7a report14429 existing references and zero moved targets.

Faithful300-input partial .text702012 unchanged. Positioned .text equal bytes
33833→33832 against728304 reference bytes; local getter gain does not imply a
positioned whole-object gain. Both strict final-object comparisons remain
DIFFERENT/exit1. Artifacts build/psd-final-integration, psd-final-audit.json,
psd-final-census.json and psd-final-phase.log. No modern portability claim.

PSD source/scalar and conversion-attribution work is concluded for this pass.
To reopen process, supply independently evidenced source/profile or pre-rename
chain differences. Do not repeat closed scalar/order controls or force the
original registers. Transfer expansion-temporary diagnosis to other measured
SIZE1 floating getters before returning to ungrounded PSD spelling searches.
