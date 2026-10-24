# Checked callee effects for a private frame argument

The jump-table prover accepts the `v22_local_loop` private-frame argument in
the native object, in the isolated per-TU object, and in the recovered-order,
300-object partial link. The blob proves its own `V22_status` body. The
isolated `v22mod.c.o` leaves `V22_status` undefined and now resolves that one
call through the partial link, whose linked body must match the unique strong
definition in the link's ordered input manifest before it is proved. All three
callers establish the same ordered 14-slot table identity.

The raw caller body still differs from the blob in the low half of the table
relocation's inline addend, which the comparator compares by relocation
target rather than by value; `V22_status` is still **SIZE, five bytes**. The
apparatus change moves the caller's grade from UNRESOLVED to EXACT because the
table identity is now provable per object. It does not make either function
source-identical, and no reconstruction source, declaration, compiler flag,
comparator policy or ratchet changes are involved.

This implements the bounded leaf route identified in
[the apparatus review](apparatus-unblock-review.md#a-bounded-next-lever-callee-effects-on-an-escaped-local-record).

## What the machine proof establishes

`tools/toolchain/callee_effects.py` decodes a limited i386 instruction subset
from the actual ELF bytes. It checks a unique, sized, strong function in the
supplied ELF32 ET_REL artifact, its normal cdecl save/restore sequence, every
reachable path and every memory store. Unknown opcodes, implicit writes,
additional calls, loops, pointer copies or mutations, and unexpected entry
paths refuse. Only exact no-op padding may be unreachable. The decoder admits
register-byte SETcc, accumulator AND/OR and exact identity LEA by their
encodings; it does not infer semantics from disassembler mnemonics alone.

The call must be an actual five-byte E8 instruction with its own
`R_386_PC32` relocation and inline addend -4. The target symbol must identify
the checked strong definition: in this artifact when the symbol is defined
there, or in the recovered-order partial link when this artifact leaves it
undefined. A same-named body is never borrowed from the blob, from another
per-TU object or from `TC_OUT`. Certificates include the artifact and body
hashes, symbol index, relocation facts, call binding and, for a linked
resolution, the manifest-selected source object. Fresh ELF bytes are checked
against cached decoding; incoming-edge scans are fresh. A changed dependency
cannot silently reuse a path-only cached proof.

The leaf tracks physical read/write byte masks separately from bit
dependencies on the record's original contents. Partial register writes,
shifts, logical operations and SETcc preserve those dependencies. A branch
whose flags depend on original record bits refuses, so storing constants on
two such branches cannot falsely establish independent initialisation.
Record-dependent external addresses refuse even if their loaded values are
later masked away. Relocated stack arguments and relocated record accesses
also refuse: their
raw displacements cannot establish their linked addresses.

The enclosing frame proof admits one frame-address origin and one checked
leaf call. It checks the exact outgoing argument slot and explicit stores
for every consumed argument. The matched pointer store must be the last
write to its argument word before CALL; full or partial replacement refuses.
The private region is disjoint from all saved,
outgoing and return slots. A second frame capability refuses. The checked
call must dominate each permitted masked read. External direct entries and
interior relocations refuse, apart from the exact jump-table relocation
fields already proved by the enclosing table analysis.

Caller-save permission does not establish a register overwrite. The leaf's
retained record pointer and any unoverwritten incoming address register stay
live until explicit full-width kills. The outgoing pointer slot must also be
overwritten before another call or return. Partial kills, copying a retained
pointer, and sending a stale slot to an unknown callee refuse.

The footprint is conditional on normal cdecl entry and valid non-record
loads. In the caller integration, separation uses the existing private-frame
invariant: no address of this live frame region has previously escaped, and
ordinary external objects do not overlap it. All other stack accesses still
pass the enclosing frame proof. This is not a proof of arbitrary external
pointer validity, source object bounds, complete callee semantics or public
modem reachability.

## Native and reconstructed witnesses

| Measurement | Native object | Recovered-order candidate |
|---|---:|---:|
| `V22_status` size | 310 bytes | 315 bytes |
| Decoded instructions, including padding | 94 | 90 |
| Reachable instructions | 94 | 89 |
| Enumerated paths | 4 | 32 |
| Distinct non-stack store instructions | 17 | 17 |
| Physical record write union | `[0,12)`, `[16,22)` | Same |
| Fully initialised record bytes | `[0,12)`, `[16,21)` | Same |
| Byte 21 initialised/preserved masks | `01` / `fe` | Same |

The native callee is `.text 0x8c450..0x8c586`. Its output pointer is loaded
into ECX at `0x8c456`. Physical stores write all of byte 21, but bits 1–7
retain their original values. A caller testing those bits would not be
licensed by the bit-0 proof.

The native caller forms frame `+0x20` at `0x8a938`, stores it in the second
argument at `0x8a948`, and calls at `0x8a94f`. The next saved slot starts at
frame `+0x4c`, yielding a **44-byte available private-frame envelope**.
The observed write extent is 22 bytes. The envelope does not establish a
44-byte C object or permit a source-bounds claim.

The callee retains ECX on return. The caller overwrites it at `0x8a958` and
clears the outgoing pointer slot at `0x8a95c` before the next call. At
`0x8a98e`, `testb $1,0x35(%esp)` consumes exactly record byte 21, bit 0.
The checked stores lie in frame `[0x20,0x2c)` and `[0x30,0x36)`, below the
saved region `[0x4c,0x5c)`.

The candidate's independently decoded body has the same masks despite its
different instruction sequence and path count. Path enumeration is a
syntactic overapproximation; it does not establish that all paths are
feasible for valid modem histories. The isolated `v22mod.c.o` has no defined
`V22_status`; the 300-object link supplies the definition and binding, and the
proof is only admitted after that linked body is matched against the unique
strong definition in the link's ordered input manifest. Linking alone does not
certify the body, and the enclosed body is proved from the link's own bytes.

## Validation and limits

The new Gentoo-assembled controls cover:

- 10 positive leaf/call cases and 44 adversarial refusals, plus two stale-cache
  negatives and the native preserved-bit negative.
- 5 positive caller/frame cases and 34 adversarial refusals, including wrong
  targets/addends, missing writes, aliases with saved/outgoing/return slots,
  extra calls, implicit pointer mutation, partial register kills, stale
  arguments, and anonymous external entries into the call or read.
- A positive extra store within the available private envelope. This records
  the proof's ABI scope: footprint safety alone does not establish source
  object extent or functional equivalence.

The existing fixed-frame suite passed 3 positive and 32 adversarial controls,
including rebased EXACT and changed-target RELOC controls and the real
34-slot/17-call V32 pair. Existing byteident checks passed 20 register cases,
59 relocation cases and 3 ratchet cases. All generated assembly used
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`.

The pre-integration frozen-object census JSON is identical across 1,852
functions: 1,144 EXACT, 36 REGALLOC, 86 BYTES and 586 SIZE, with identical
ordered EXACT membership. All 300 canonical object hashes remain unchanged, and the final
census used the recorded, unchanged tool snapshot. `make refs` also passed
6 regression tests, 12,966 references and 3,100 finding headings, with no
unresolved, pending or stale references. No source-runtime differential is claimed for these apparatus
changes; the meaningful checks are the adversarial ELF controls, actual
artifact proofs and unchanged comparison census.

**Pre-integration note (`2c72de0e9`).** Before cross-TU resolution the
conservative census was 1,145 EXACT / 1 UNRESOLVED / 36 REGALLOC / 86 BYTES /
584 SIZE. `v22_local_loop` was the UNRESOLVED symbol when graded per object:
`V22_status` is defined in the separate `v22stc.c.o`, so
`private_frame_effects` refused an undefined strong callee. The same proof
already succeeded against the recovered-order partial link
(`build/partial/dsplibs.o`), so the gap was cross-TU callee resolution in the
comparator, not a table or frame fact.

## Cross-TU callee resolution

`tools/toolchain/callee_effects.py` resolves an **undefined** global call
target through the recovered-order partial link only. A target defined in the
supplied per-TU ELF is still proved from that ELF; the link is never consulted
in that case, so a same-named linked body cannot displace a local definition.

The resolution refuses unless all of the following hold:

- the per-TU relocation names one undefined `STB_GLOBAL` `STT_FUNC` target and
  its own `R_386_PC32` addend is still `-4`;
- `link` and `manifest` are supplied (the defaults are
  `build/partial/dsplibs.o` and `build/tc_repro/tc_link_manifest.txt`) and the
  link is not the artifact being proved;
- the link defines exactly one strong, sized `STT_FUNC` of that name, of
  `STB_GLOBAL` binding;
- the manifest names exactly one input object with a strong, sized definition
  of the name, and the linked body matches that object's body. Matching masks
  every relocation's four addend bytes, requires the non-relocated bytes to be
  equal, requires the relocation offsets/types/target sections to be equal,
  and requires each target section's addend shift to equal the rebase implied
  by that section's **actual placement** in the link. The placement is
  reproduced from the ordered manifest's input section headers (each same-named
  section concatenated at its own alignment) and must explain the linked output
  section's whole size. A section-symbol target moves by its input section's
  placement offset; a named-symbol target defined in the input keeps its
  addend while the symbol itself moves by that offset, and an undefined
  reference may instead resolve to a definition in the link without changing
  its addend. A uniform but stale delta, a missing or ambiguous section, a
  manifest that does not explain the linked section, or a differently
  coded/rebound body refuses.

The same `artifact()` and `prove()` code then prove the linked body; nothing
about the proof's instruction, path, store or mask checks is relaxed. The
integration point is the existing `byteident.body()` jump-table path, which
passes the link context into `jumptable.prove()`. The blob and every artifact
without a link context keep the previous refusal.

For the real `v22_local_loop` proof, the selected `v22stc.c.o` relocation in
`V22_status` has addend `0` against `.rodata`. The ordered-manifest layout puts
that input's `.rodata` at **24,948 (0x6174)**, and the linked relocation addend
is also `0x6174`, so the observed delta equals the section placement. As an
independent witness, the linked `.rodata` bytes at `0x6174` are exactly the
selected copy's 14 bytes (`0300000001000200070008000500`). A uniform value alone
would not have established this.

Proof parameters for the reconstructed caller: fixed frame 92 bytes,
private-frame envelope **44 bytes**, 14 table entries, callee **315 bytes**,
90 decoded / 89 reachable instructions, **32 enumerated paths**, **17 distinct
non-stack stores**, write union `[0,12)`, `[16,22)`, initialised `[0,12)`,
`[16,21)`, and byte 21 initialised bit 0 / preserved bits 1–7. The blob's
per-TU proof remains 310-byte callee, 94 instructions, 4 paths, same masks.

Census after the integration, over 1,852 functions:
**1,146 EXACT / 0 UNRESOLVED / 36 REGALLOC / 86 BYTES / 584 SIZE**. The ordered
EXACT-set difference from the pre-integration census is exactly
`+ v22_local_loop`, with no losses. `byteident --ratchet` passes
(1,112 → 1,146), and `byteident --link-selected` is unchanged for its three
disagreeing names and reports 32/32 selected linked bodies matching an input
copy.

Reproduction from a tree with canonical Gentoo objects:

```sh
make tc J=4
make partial-link J=4
python3 tools/toolchain/callee_effects_controls.py
python3 tools/toolchain/callee_frame_controls.py
python3 tools/toolchain/jumptable_fixed_setcc_controls.py
python3 tools/toolchain/callee_effects.py ref/slmodemd/dsplibs.o V22_status --capacity 44
python3 tools/toolchain/jumptable.py ref/slmodemd/dsplibs.o v22_local_loop
python3 tools/toolchain/byteident.py --ratchet
python3 tools/toolchain/byteident.py --link-selected
python3 tools/toolchain/byteident.py --list-exact --json-out scratch/after-census.json
```

`make byteident` and `make byteident-ratchet` depend on `partial-link`, so the
census resolves the undefined callee; running `byteident.py` directly without
the link leaves that grade UNRESOLVED by design.

`callee_effects_controls.py` now also assembles and partially links synthetic
two-TU fixtures (with the Gentoo image's `as` and `ld`) and checks the real
per-TU `v22_local_loop` artifact. It requires `build/tc_out` and
`build/partial`; the call instruction is `0x21bf` in `v22mod.c.o` and its
relocation field is `0x21c0`. The existing fixed-frame controls invoke `as`;
route that executable through the Gentoo image when replaying them. Raw
generated fixtures and certificates belong in `scratch/callee-effects/`, not in
the committed source tree.

Control denominators: 10 positive leaf/call + 44 adversarial + two stale-cache
negatives + the native preserved-bit negative, plus **10 cross-TU controls** —
three link-resolved positives (one with a zero-placement `.text` body, one
whose `.rodata` is placed at offset 16, and one whose undefined `exttable`
reference resolves to a `.rodata` definition in another input without changing
its addend), six refusals (missing from the link, a linked body writing past
the proved 8-byte capacity, a linked body that differs from the
manifest-selected copy, a changed addend, a uniform-but-stale addend, and a
manifest that omits a section contribution the link contains), and one
per-object definition that is proved from its own ELF while the supplied link
holds a different body. A separate one-case refusal checks a target section
name absent from the selected copy. The positives assert the certificate's
artifact hash and `callee_rebase`; the rebased positive also asserts that the
linked `.rodata` bytes at the certified placement equal the selected copy's
bytes, and the per-object case asserts the certificate is the per-TU object's.

The changed-addend and stale-uniform fixtures are measured changes, not
tautologies: a control predicate reproducing the pre-change check (masked body,
relocation shape, per-target uniform delta) accepts both patched links, while
`_same_linked_body` refuses each because the delta disagrees with the section
placement. The missing-layout fixture is likewise accepted by the old
predicate and refused because the manifest layout cannot explain the linked
section's size.

## COMDAT section targets

#357's supplementary scan proved `_same_linked_body` for all 1,726 names with
exactly one strong, sized `STT_FUNC` definition among the manifest inputs. It
passed 1,661 and refused 65. The measured composition is **64 COMDAT refusals
in 19 distinct `.gnu.linkonce.t.*` groups plus one ordinary-section refusal**.
The 64 are named-symbol relocations whose target function is defined in a
COMDAT section, not section-symbol relocations to one; all **319** references
to linkonce-defined targets in the 300-input tree are named-symbol references.
The 65th is `GetDialerConfig`: an `R_386_32` against ordinary `.rodata` whose
source addend is `-12`, a section-relative base for an indexed table. Its
observed linked addend is placement minus 12, but the existing check read the
addend unsigned and compared the resulting non-modular delta. That ordinary
case is independent of COMDAT and is resolved by the signed addend path below.

### Kept-copy evidence

The 300 manifest inputs contain **119 `.gnu.linkonce.t.*` section occurrences
forming 85 distinct names**; 32 names have more than one contributing object.
The partial link contains 85 unique `.gnu.linkonce.t.*` sections, so `ld -r`
deduplicated the groups rather than concatenating them. For every one of the
85 groups the linked section's masked bytes, relocation shape, size and
alignment equal the **first contributor in the manifest's order**. That is an
observation, not an assumption: four groups have copies that differ outside
relocated fields (different register allocation), and for those the linked
bytes match the first copy and differ from the later one. The blob is the
independent control: it is a deduplicated ET_REL artifact with 79 unique
`.gnu.linkonce.t.*` and 4 unique `.gnu.linkonce.r.*` sections and no duplicate
name.

### Model and refusals

`_section_placement()` now routes a `.gnu.linkonce.t.*` name to
`_comdat_section_placement()`. Name-based COMDAT keeps the first contributing
section, so the kept group occupies offset **0** of its own linked output
section. Before returning that placement the resolver requires:

- the manifest-selected copy contributes the named section;
- every contributor agrees on section size and alignment (otherwise the group
  is ambiguous);
- the linked object contains exactly one section of that name, of the first
  contributor's size and alignment;
- the linked section's masked bytes and relocation shape equal the **first**
  contributor's.

A missing group, an ambiguous or duplicate linked section, disagreeing
contributor sizes/alignments, or linked bytes that are not the first copy all
refuse. Later copies are deliberately not required to be byte-identical:
register allocation can differ between translation units, and the linker keeps
the first regardless. The existing named-symbol check still requires the linked
symbol value to move by this placement, so a symbol at a different offset in
the kept copy is refused. Ordinary section names and the other `.gnu.linkonce.*`
kinds keep the previous concatenation path unchanged.

### Controls

`callee_effects_controls.py` adds synthetic Gentoo-assembled COMDAT fixtures:
**1 positive and 6 refusals**, plus **1 pre-change refusal**. The positive has
two contributors to `.gnu.linkonce.t.comdat_probe` and proves the *discarded*
source copy; the link keeps the first, and the certificate reports
`symbol_rebases['comdat_probe']` with the group as both source and resolved
section, value delta 0 and addend delta 0. The linked group bytes are asserted
equal to the first copy and different from the source copy. The refusals are:
the link keeps the second copy while the manifest names the first; a changed
byte in the linked group; contributors that disagree on size; a linked size
that disagrees with the manifest's first copy; a duplicated linked section
name; and a missing linked group. A replica of the old concatenation predicate
(sum every contributor; require the sum to equal the linked size) refuses the
positive, which is the measured pre-change behaviour.

The 65 refusals reduce to 64 COMDAT cases resolved here and the one ordinary
negative-addend case resolved by the signed addend path below; the pre-change
code refused all 65.

## Signed section-relative addends

The i386 relocation addend is two's-complement, not an unsigned offset. The
source `GetDialerConfig` carries `.rodata` relocations with addends `-12` and
`+12`, and `ld -r` writes the linked fields as `placement-12` and
`placement+12`. `_function_identity()` now sign-extends the four-byte field,
and `_same_linked_body()` compares the linked addend with the expected signed
value: `placement + source_addend` for a section-symbol target, or the
unchanged `source_addend` for a named symbol. Reading either side unsigned
changes the value by `2**32` and made the correct negative-addend case refuse.
The named-symbol and COMDAT rules are unchanged, and a genuinely wrong
placement still refuses.

For the real case, the manifest-selected
`build/tc_repro/src_dialer_DialerConfig.c.o` has `.rodata` addends
`[-12, 12]`; the linked fields are `[18404, 18428]`, and the ordered manifest
places that input's `.rodata` at `18416 (0x47f0)`, so
`18416-12 = 18404` and `18416+12 = 18428`. The positive now proves, and the
supplementary scan over all 1,726 names with exactly one strong, sized
`STT_FUNC` definition passes **1,726/1,726**: the COMDAT model accounts for the
previous 64 refusals and this for the 65th.

Controls add two synthetic Gentoo-assembled positives (`table-12` and
`table+8` with a 16-byte placement) and one refusal, a patched linked addend
whose uniform delta 28 disagrees with the placement 16. Two replicas of the
pre-change reading -- both addends unsigned, and the source-unsigned /
linked-signed mix -- refuse the correct negative-addend fixture and the real
`GetDialerConfig`, while the consistent signed reading accepts both. This
makes the acceptance a measured difference from the old code rather than a
claim about it.

The frame suite passes 39/39, fixed-SETcc 135/135, the fixed-ABI suite 35/35
with the real V32 pair EXACT, the base jumptable suite 47 synthetic/2 real,
induction 20/20, `byteident --self-test` 79 checks (20 register, 59 relocation)
and `byteident --ratchet-self-test` 28/28. The census is unchanged at
**1,146 EXACT / 0 UNRESOLVED / 36 REGALLOC / 87 BYTES / 583 SIZE** (grade 0 or
1: 1,182; the base at `366eb7f0d` measures the same 87 BYTES), the ratchet
passes, link-selected EXACT remains **1,149** with 32/32 selected bodies
matching an input copy, and the blob sha256 remains
`1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379`.

## Recommendations after the tests

Use the checked partial-link boundary when investigating this caller's
remaining real byte differences. Keep the leaf footprint certificate beside
the source investigation: matching masks support frame safety, while the actual
caller and callee bytes remain separate reconstruction obligations.

Extend the leaf domain only for a measured next instruction or caller shape,
with a negative control for each new assumption. Transitive callees, loops,
multiple live frame records, arbitrary entry paths and unbounded aliases
remain explicit refusals. These tests do not justify extending the result to
those domains. Cross-TU resolution is not a general same-name lookup: it is a
closed-manifest, unique-strong-binding, body-matched exception for the one
undefined global leaf call.
