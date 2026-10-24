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
  branch. The parser now accepts that adjacent comparison. The independent
  frame obligations remain: the caller stores already-saved EBX/ESI/EDI as
  outgoing arguments, which the save classifier reads as duplicate saves,
  and it passes a frame-local record beginning at `+0x10` to
  `RingDetector_Create` (`0x21e9..0x21f0`), which calls
  `sysdep_malloc`/`dsplibs_debug_printf` and is not a bounded leaf.
  Generalising the guard parser alone therefore does not close the function
  and moves no grade.

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
object both pass the guard now and stop at the frame refusal described above.

The complete comparison census is identical to the frozen baseline across
1852 functions: **1144 EXACT, 36 REGALLOC, 86 BYTES and 586 SIZE**, with zero
RELOC, UNRESOLVED or NODATA. The ordered exact-symbol membership and all
other JSON census fields are identical. The 14 same-size pairs contain no
occurrence of the newly recognised guard pattern; their existing grades
remain unchanged. The byte comparator also passes its 59 relocation controls
and three exact-set ratchet controls.

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
