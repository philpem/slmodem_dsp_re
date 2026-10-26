# Jump-table word guards and refusal triage

The jump-table prover now recognises an unsigned 16-bit comparison followed
by a same-register zero-extension before an absolute dispatch. This proves
the anonymous tables in both versions of `interpretMohTimeout`, while
preserving their different ordered destinations and their **SIZE 34**
comparison. No reconstruction source or acceptance threshold changes.

This investigation uses base `60804df5b37956fc20fccfa4dfb9ded282fd9711` and
its frozen 300-object Gentoo GCC 3.4.2-r2 build. All generated ELF controls
use the assembler in
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`:
GNU assembler 2.15.92.0.2, dated 20040927.

## Separate the refusal from the code difference

An opcode prefilter finds 78 native functions containing `ff 24`. This is a
candidate inventory, not proof of a jump table. Excluding the separately
owned source investigations removes two matches, leaving 76 functions.
The previous ordinary proof accepts 10; its induction fallback accepts one
more. The remaining 65 have unsupported proof shapes. The most frequent
first refusal is an unsupported ABI data-relocation field (17 functions).
A first refusal identifies where analysis stopped; it does not establish
that all later obligations would pass.

Fourteen of the 76 native functions have a same-size canonical candidate.
Nine already have an ordinary or induction proof. The other five all have
independent non-relocated byte differences:

| Function | Size | Differing non-relocated bytes | First ordinary refusal |
| --- | ---: | ---: | --- |
| `v22_local_loop` | 1104 | 4 | Unsupported fixed-frame instruction |
| `RD_create` | 293 | 11 | Unsigned immediate/register guard |
| `VPcmFloModem::v90RunDemodulator` | 3013 | 131 | Additional indirect control transfer |
| `fax_class1_command` | 615 | 49 | Unsupported ABI data relocation field |
| `voice_command` | 802 | 18 | Unsupported fixed-frame stack access |

Proving an anonymous table cannot remove these byte differences. Each pair
continues to report BYTES at the byte-comparison stage. Neither source rows
nor case meanings are paired existentially to improve that verdict.

Two native examples show why extending the first instruction whitelist is
insufficient:

- `v22_local_loop` first refuses `sete %al` at `0x8a7ea`. It later passes
  the address of frame `+0x20` to `V22_status` (`0x8a938..0x8a94f`) and reads
  frame byte `+0x35` at `0x8a98e`. A useful broader frame proof needs a
  bounded callee write-footprint and ownership of that local record, including
  its separation from saved registers. Accepting SETcc alone would not prove
  those obligations.
- `RD_create` has a taken `jbe` at `0x21de` leading to its dispatch at
  `0x2210`. Its comparison is immediately adjacent to the branch
  (`sub $4,%eax; cmp $0xb,%eax; jbe`), a shape the taken-edge parser did not
  admit because it assumed a flag-preserving store between comparison and
  branch. The parser now accepts that adjacent comparison. Two frame
  obligations followed. The first is now modelled: the caller stores a
  redefined ESI into its stack record, which the old save classifier read as
  a duplicate save (see "Frame stores versus saves" below). The second
  remains, and refuses: the record beginning at `+0x10` is passed to
  `RingDetector_Create` (`0x21e9..0x21f0`), which calls
  `sysdep_malloc`/`dsplibs_debug_printf` and is not a bounded leaf. So the
  guard and the frame classifier are both accepted and the function still
  moves no grade.

These are next proof obligations, not findings that the code is unsafe or
that a source route is exhausted.

## Independent word-guard witness

The native `_Z19interpretMohTimeouts` starts at `0x6190` and occupies
102 bytes. Its dispatch is:

```asm
6195: cmp    $0xd,%ax
6199: ja     61a5
619b: movzwl %ax,%eax
619e: jmp    *0x260(,%eax,4)
```

The original table has 14 ordered relocations. The current canonical
function has the same guard/extension pattern, but occupies 136 bytes and
has different relative destination offsets. Before this change both proofs
refused at the guard parser. Afterwards both establish the table extent and
all ordered instruction targets; the identities remain different.

For a register value `R`, let `u = R & 0xffff`. The unsigned comparison's
fallthrough establishes `u <= bound`. The decoded MOVZX assigns `R = u`,
erasing every upper bit that the comparison did not inspect. Therefore the
32-bit dispatch index lies in `0..bound`. The existing table-size limit
requires `bound + 1` to lie in `1..256`.

The extension accepts only exact unprefixed register MOVZX bytes, with the
same low word and destination register as the comparison. The supported
comparison encodings are operand-size-prefixed `83 /7`, `81 /7` and the AX
accumulator form. The sign-extended immediate form is restricted to
non-negative bytes. ESP is excluded. Signed comparisons, a different source
or destination, extra instructions, unsupported prefixes and out-of-domain
bounds refuse.

The existing protected corridor now includes the zero-extension. Named,
direct, relocated or table entries that bypass the comparison/guard or enter
the extension/dispatch still refuse. Table storage, extent, relocation kind,
ordered slot identity, instruction boundaries and ABI-frame checks remain
mandatory. This does not introduce matching by case value, by mnemonic
similarity or by an arbitrary permutation of destinations.

## Controls and collateral

`tools/toolchain/jumptable_word_guard_controls.py` assembles its fixtures
inside the Gentoo container and reports its denominator. It passes:

- 16 positive ELF controls, including dirty upper bits, all seven supported
  register encodings (with ABI saves for callee-save registers), one-entry and 256-entry tables, both immediate-width encodings,
  relocation rebasing and the existing 32-bit guard domain.
- 26 adversarial refusals covering missing/signed/mismatched extensions,
  bounds, flags, index/scale changes, prefixes, SP guards and alternate entries.
- A rebased table remains EXACT; reordered or duplicated slots report RELOC.
  Every refused fixture remains UNRESOLVED even when compared with itself.
- Both real 14-slot tables prove; their comparison remains SIZE 34.

The existing fixed-frame ABI suite also passes: three positive controls,
32 adversarial refusals, rebased EXACT, altered targets RELOC, and the real
34-slot/17-call `V32LocLoopNextState` pair EXACT. Its fixture previously used
`mov $message,4(%esp)`, which Gentoo's assembler refuses because the operand
width is absent. The fixture and its two negative-control anchors now use
explicit `movl`. This is a test-apparatus correction.

`tools/toolchain/jumptable_adjacent_guard_controls.py` measures the
taken-edge adjacent-comparison form by itself. It passes 9 positives (rebased
and unrebase indices, three registers, one- and 256-entry tables, a rebased
table, swapped targets and the existing corridor form) and 14 refusals
(signed and opposite guards, wrong and truncated extents, an ambiguous guard,
a clobbered-index corridor, a guard target that is not the dispatch, a guard
after the dispatch, a non-register comparison, a second indirect jump, wrong
dispatch register or scale, and named/table guard entries). It also runs the
prover from the branch point
(`2a9d62fc5:tools/toolchain/jumptable.py`), which refuses both new positives
with `unsigned immediate/register guard`; that is the measured pre-change
behaviour rather than a replica. For `RD_create` the blob and the period
object both pass the guard now and stop at the non-leaf frame refusal
described below. The suite resolves the period object through the
recovered-order partial link (as `byteident` does) so both sides report the
same residual; without the link it records the link-requirement refusal
instead.

The complete comparison census is identical to the frozen baseline across
1852 functions: **1144 EXACT, 36 REGALLOC, 86 BYTES and 586 SIZE**, with zero
RELOC, UNRESOLVED or NODATA. The ordered exact-symbol membership and all
other JSON census fields are identical. The 14 same-size pairs contain no
occurrence of the newly recognised guard pattern; their existing grades
remain unchanged. The byte comparator also passes its 59 relocation controls
and three exact-set ratchet controls.

## Frame stores versus saves

The fixed-frame save classifier treated every pre-dispatch
`mov %reg,off(%esp)` of a callee-save register as a save. `RD_create`
redefines ESI from its incoming argument (`mov 0x44(%esp),%esi`, blob
`0x213e`) and then stores that computed value into its stack record
(`mov %esi,0x10(%esp)`, blob `0x21a9`), well before the taken dispatch at
`0x2210`. EDI is redefined at `0x2146` and stored the same way as the call's
argument (`mov %edi,(%esp)`, `0x21d0`), also before the dispatch. The old
classifier saw second saves of already-saved registers and refused
`saved slot duplicate/clobber` at the first of them; that was `RD_create`'s
first refusal in the previous wave.

The rule now:

- A **save** is a pre-dispatch 32-bit `mov` of a callee-save register
  (EBX/ESI/EDI/EBP) into a 4-byte-aligned word wholly inside the allocated
  frame, where the register still holds its entry value -- it has not been
  redefined since entry -- and neither the register nor the slot already has
  an owner. Uniqueness is per register and per slot: two saves of one
  register, or two owners of one slot, still refuse
  `saved slot duplicate/clobber`.
- Any other store into an in-frame word -- an immediate, a caller-save
  register, or a callee-save register that has been redefined -- is a
  **computed-value frame write**. It keeps the outgoing extent rules
  (`mov`/`movl`, 4-aligned, `offset+4<=frame`) and is still refused if the
  word it writes belongs to a save (`fixed-frame outgoing/save overlap`). It
  is not a save and never becomes a restore source.

No new capability is admitted. The computed store was already admitted as an
outgoing argument whenever the register was not saved -- the outgoing operand
list includes EBX/ESI/EDI/EBP -- and when the register *was* saved it was
refused only because the duplicate-save test fired, not because the store was
unsafe. The saved slot still holds the entry value and is the only restore
source, the frame is still released before RET, and `written <= saved` still
binds.

The frame prover now records, per frame word, the set of stores that wrote
it, and the private-frame model decides liveness from reachability rather
than from offset alone. A word is protected (it bounds the record and refuses
an alias) only if one of its stores can execute at or after the private
record's address is taken; a word whose only stores are unreachable from that
origin is dead argument space or the record's own initialisation. In
`RD_create` the six record fields are written through the frame base before
the `lea 0x10(%esp)` at `0x21e9`, so they no longer bound the record; the
record's capacity becomes 32 bytes (to the ESI save at `+0x30`) and the
regions below and above still refuse when they are live. `v22_local_loop`
keeps capacity 44 because its outgoing stores are re-reachable from the
origin through the loop's backward edges. See
[callee-effects-proof.md](callee-effects-proof.md) for the reachability
controls.

### Controls and measured pre-change refusal

`tools/toolchain/jumptable_fixed_abi_controls.py` grows from 3 positives /
32 refusals to **4 positives / 35 refusals**:

| control | expected | reason |
| --- | --- | --- |
| `redifined-save-store` (new positive) | accept | pre-dispatch store of redefined ESI into a free word |
| `duplicate-save` | refuse | `saved slot duplicate/clobber` |
| `duplicate-save-other-slot` | refuse | second save of the entry value into other slot |
| `redifined-save-store-overlaps-save` | refuse | `fixed-frame outgoing/save overlap` |
| `redifined-save-store-unaligned` | refuse | `fixed-frame outgoing extent/form` |

`tools/toolchain/callee_frame_controls.py` grows from 5 positives / 34
refusals to **6 positives / 35 refusals** with `dead-outgoing-area-record`
(record written before its address is taken, accepted) and
`post-origin-store-into-record` (the same alias made by a store reachable
from the origin, still `private record aliases outgoing/save/return slots`).
Both suites assemble with the Gentoo image's `as`.

The acceptance is a measured change, not a replica. The prover and callee
model at the base commit refuse both new positives:

```sh
mkdir -p /tmp/pre
git show 68d23eb09:tools/toolchain/jumptable.py > /tmp/pre/jumptable.py
git show 68d23eb09:tools/toolchain/callee_effects.py > /tmp/pre/callee_effects.py
PYTHONPATH=/tmp/pre python3 /tmp/pre/jumptable.py \
  build/jumptable-fixed-abi-controls/redifined-save-store.o sample
# UNPROVED: saved slot duplicate/clobber
PYTHONPATH=/tmp/pre python3 /tmp/pre/jumptable.py \
  scratch/callee-effects/frame-controls/dead-outgoing-area-record.o caller
# UNPROVED: private record aliases outgoing/save/return slots
```

### `RD_create`'s remaining refusal, and why it stays UNRESOLVED

With the classifier and the origin-relative live set, `RD_create` advances
through two measured refusals:

| stage | refusal |
| --- | --- |
| base | `saved slot duplicate/clobber` (save classifier) |
| classifier fixed | `private record aliases outgoing/save/return slots` |
| origin-relative live set | `unsupported callee opcode (including calls/implicit writes)` |

The last one is the non-leaf callee, and it is where the function must stop
under the present model. The evidence:

- `RingDetector_Create` (blob `0x2550`, per-TU `0x1d0`) contains
  `call sysdep_malloc` (`0x1e2`) and `call dsplibs_debug_printf` (`0x351`).
  Both targets are `U` in `ref/slmodemd/dsplibs.o`: there is no body in this
  object to prove. `callee_effects.decode` refuses a call opcode by design,
  and `prove()` is documented and bounded as an acyclic leaf proof.
- The caller-side capability obligation is also unmet. After the call at
  blob `0x21f0`, EDX (which held the record address) is never written again
  on any path to RET, and the failure path reaches the `sysdep_free` call at
  `0x221c` without a kill. The object's bytes are identical
  (`0x21f5..0x224f`), so this is not a reconstruction difference.

Option (b) as posed -- treat the outgoing pointer slot as clobbered by the
call and require no read afterwards -- is measurable and true here: the only
later call is `sysdep_free`, and slot `0` is overwritten at `0x2219` before
it. But it is necessary and not sufficient. The model also requires the
register capability to be destroyed, and above all it requires a proved
callee contract; accepting an unproved callee would relax the leaf invariant
itself. Option (a) -- modelling `RingDetector_Create`'s effect on the escaped
record without a full leaf proof -- would need a new transitive
external-call/escape proof (that neither call's arguments reach the record)
plus the caller-side liveness change, each with its own adversarial controls.
Neither is done here, so `RD_create` remains the single UNRESOLVED symbol and
moves no grade. Its linked body is still byte-identical in the link-selected
view.

## Current census at this change

Comparison over 1,852 symbols: **1,147 EXACT, 1 UNRESOLVED, 0 RELOC,
37 REGALLOC, 87 BYTES, 580 SIZE** (grade 0 or 1: 1,185) -- the same buckets as
the pre-change tree, because `RD_create` stays UNRESOLVED. `make period J=3`
reports **394 passed, 0 failed**. `byteident --ratchet` is OK (exact
1146 -> 1147, grade 0-or-1 1182 -> 1185, link-selected exact 1149 -> 1150,
regalloc 36 -> 37, the single reviewed `dp_vpcm_exit` displacement unchanged);
link-selected resolves 32 multiply-defined names and 32/32 linked bodies match
an input copy. `byteident --self-test` passes 20 register and 59 relocation
cases; `--ratchet-self-test` passes 31 cases with no failure. The blob is
still `1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379`.

## Reproduction

With a complete canonical Gentoo build available:

```sh
python3 tools/toolchain/jumptable_word_guard_controls.py \
  --candidate build/tc_out/src_pump_v34_VPcmV34Main.cpp.o
```

The existing ABI suite uses `as` from PATH; route that executable through
the Gentoo image before running:

```sh
python3 tools/toolchain/jumptable_fixed_abi_controls.py \
  --candidate build/tc_out/src_pump_v32_V32loop.c.o
```

The adjacent-comparison suite reads `RD_create` from the period tree and the
pre-change prover from the branch point:

```sh
python3 tools/toolchain/jumptable_adjacent_guard_controls.py \
  --candidate build/tc_out/src_service_rd.c.o
```

The investigation worktree is `/tmp/slmodem-jumptable-unblock`.
`scratch/native-proof-census.json` retains the 76-function inventory;
`scratch/unproved-same-size.json` records the five independent byte differences;
`scratch/word-guard-controls/results.json` contains all new proof/control
results and the exact assembler command. The full native and candidate
ordered target lists are retained there. These records supplement the native
addresses and reproduction commands above.

## The fallthrough index-copy corridor (F12054)

A fixed-frame switch may copy the compared selector before an unsigned
fallthrough guard:

    cmp $0x11,%eax
    mov %eax,%esi
    ja  default
    jmp *table(,%eax,4)

The prover now admits exactly one unprefixed `MOV r32,r32` here when it
decodes as `0x89 /r`, its source is the compared register, its destination is
not, and the dispatch uses the compared register or the copy. The copy lies
inside the existing protected range, so extent, relocation-form,
protected-corridor and single-indirect-jump checks are unchanged. The ABI
data-relocation whitelist also decodes the exact 32-bit SIB/disp32 load
(`8b /r`, base disp32, index not `%esp`) so `v8StatusName`'s relocated field
in `v8_process` is recognised.

`v8_process` now proves on both the blob and the candidate (18 ordered
destinations, equal identity) but stays **BYTES (23)**: grade 1 stops earlier
at the `setne %dl`/`setne %cl` partial-width row, which the deliberate
`_full_write` rule must reject (finding F7762, and the correction in F12054).
Proving the table is therefore necessary but not sufficient for that symbol.

Controls: `python3 tools/toolchain/jumptable_copy_corridor_controls.py`
(8 positives, 14 adversarial refusals; the base-commit prover refuses the four
new positives with `missing adjacent guard` and still proves the adjacent and
pushed-corridor forms). Census at this change over 1,852 symbols: **1,154
EXACT / 1 UNRESOLVED / 35 REGALLOC / 89 BYTES / 573 SIZE**; `make period J=3`
**394 passed, 0 failed**; `byteident --ratchet` OK; blob sha256 unchanged.
