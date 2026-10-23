# Checked callee effects for a private frame argument

The jump-table prover now accepts the `v22_local_loop` private-frame argument
in the native object and in the recovered-order, 300-object partial link.
Each artifact supplies and independently proves its own `V22_status` body.
Both callers establish the same ordered 14-slot table identity. The actual
comparison remains **BYTES, four differing bytes** for `v22_local_loop` and
**SIZE, five bytes** for `V22_status`; this apparatus change does not make
those functions exact.

This implements the bounded leaf route identified in
[the apparatus review](apparatus-unblock-review.md#a-bounded-next-lever-callee-effects-on-an-escaped-local-record).
No reconstruction source, declaration, compiler flag, comparator policy or
ratchet changes are involved.

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
the checked strong definition in that same artifact. An undefined per-TU
symbol cannot borrow a same-named body from another file or from `TC_OUT`.
Certificates include the artifact and body hashes, symbol index, relocation
facts and call binding. Fresh ELF bytes are checked against cached decoding;
incoming-edge scans are fresh. A changed dependency cannot silently reuse a
path-only cached proof.

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
feasible for valid modem histories. The isolated `v22mod.c.o`
still refuses because it has no defined `V22_status`. The 300-object link
supplies the actual definition and binding; linking alone does not certify
the body.

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

The final frozen-object census JSON is identical across 1,852 functions:
1,144 EXACT, 36 REGALLOC, 86 BYTES and 586 SIZE, with identical ordered EXACT
membership. All 300 canonical object hashes remain unchanged, and the final
census used the recorded, unchanged tool snapshot. `make refs` also passed
6 regression tests, 12,966 references and 3,100 finding headings, with no
unresolved, pending or stale references. No source-runtime differential is claimed for these apparatus
changes; the meaningful checks are the adversarial ELF controls, actual
artifact proofs and unchanged comparison census.

**Integration note (`2c72de0e9`).** On the current master the conservative
census is 1,145 EXACT / 1 UNRESOLVED / 36 REGALLOC / 86 BYTES / 584 SIZE.
`v22_local_loop` remains the UNRESOLVED symbol when graded per object:
`V22_status` is defined in the separate `v22stc.c.o`, so
`private_frame_effects` refuses an undefined strong callee. The same proof
succeeds against the recovered-order partial link
(`build/partial/dsplibs.o`), so the remaining gap is cross-TU callee
resolution in the comparator, not a table or frame fact. These apparatus
changes move no grade on their own.

Reproduction from a tree with canonical Gentoo objects:

```sh
python3 tools/toolchain/callee_effects_controls.py
python3 tools/toolchain/callee_frame_controls.py
python3 tools/toolchain/callee_effects.py ref/slmodemd/dsplibs.o V22_status --capacity 44
python3 tools/toolchain/jumptable.py ref/slmodemd/dsplibs.o v22_local_loop
python3 tools/toolchain/byteident.py --list-exact --json-out scratch/after-census.json
```

The existing fixed-frame controls invoke `as`; route that executable through
the Gentoo image when replaying them. The new control scripts select the
Gentoo image themselves. Raw generated fixtures and certificates belong in
`scratch/callee-effects/`, not in the committed source tree.

## Recommendations after the tests

Use the checked partial-link boundary when investigating this caller's four
remaining real byte differences. Keep the leaf footprint certificate beside
the source investigation: matching masks support frame safety, while the
actual caller and callee bytes remain separate reconstruction obligations.

Keep undefined per-TU calls unresolved. A future cross-object resolver would
need a closed link manifest, unique strong binding, exact call addends and
fingerprinted dependencies. A same-name lookup is insufficient.

Extend the leaf domain only for a measured next instruction or caller shape,
with a negative control for each new assumption. Transitive callees, loops,
multiple live frame records, arbitrary entry paths and unbounded aliases
remain explicit refusals. These tests do not justify extending the result to
those domains.
