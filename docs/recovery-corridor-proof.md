# Argument-load corridor and push-only table proof

The bounded jump-table prover now supports one flag-preserving incoming
argument load between CMP and JA, and a pushed-register leaf frame with no
outgoing allocation. These two independently testable extensions establish
`V90ConstellationPower::getConstellationInfo`'s ordered table identity on both
the reference and the unchanged period object.

The declared hypothesis was that the sole interposed MOV preserves both the
selector and EFLAGS, and that the entry EBX push/pop pair protects the ABI
without allocating an outgoing area. A selector write, flag change, relocated
load, guard bypass, saved/return-slot access, unbalanced restore, or call in
this push-only frame is a falsifier and must remain refused.

## Supported forms

The corridor contains exactly one unprefixed `MOV disp8/disp32(%esp),r32`.
The destination must differ from the selector and ESP. The decoded opcode,
ModRM mode, ESP SIB, and length are checked; the frame proof independently
requires an aligned incoming-argument offset above the saved registers and
return word. Existing relocation-field rules refuse load overlays. The
protected guard interval still begins immediately after CMP and includes the
interposed MOV, JA, and dispatch, so direct or relocated incoming bypasses
remain unsupported. No arbitrary instruction motion is accepted.

A push-only frame retains the unique entry callee-register saves and requires
balanced reverse-order pops on every reachable return. It provides no private
or outgoing stack area. Calls are explicitly unsupported. Argument access is
valid only after all entry saves and before restoration; stack writes,
escaping stack addresses, saved-slot reads, and return-word reads are refused.
The existing allocated-frame proof retains its allocation/release checks.

Both real objects have ordered targets at function offsets 29, 88, 110, 132,
154, and 66, with 65 reachable instructions, one return, one EBX save, no
calls, and zero outgoing bytes. Production `body`/`verdict` reports EXACT.

## Controls and provenance

```sh
TC_OUT=build/tc_repro python3 tools/toolchain/jumptable_corridor_controls.py
```

The synthetic controls independently exercise push-only frames, adjacent
CMP/JA, disp8 and disp32 corridors, two saves, and the existing allocated
frame combined with the corridor. Five positive controls are EXACT.
Twenty-seven invalid controls refuse and remain UNRESOLVED through the shared
verdict. These include flag/selector changes, partial/prefixed/indexed loads,
multiple interposed instructions, unsupported memory loads, unsaved register
writes, unaligned loads, saved/return-slot reads and writes, frame escape,
calls, zero-sized allocation/release, post-pop argument accesses, wrong/missing restores, post-restore clobbers, direct guard/corridor
bypasses, incoming relocations to the corridor, and relocation overlays.
An altered ordered table yields RELOC.

The SETcc controls also include unsaved BL/BH writes, bringing their refused
case count to fourteen. Existing fixed-frame and shared comparator integration
controls were rerun after this extension. No reconstruction source, flags,
objects, or ratchet were changed. The parent integration owns `make phase`.

This uses the same frozen 300 objects, reference, and hash-checked local copy
as the [SETcc proof](recovery-setcc-proof.md). Artifacts are in
`build/jumptable-corridor-controls/results.json` and `build/setcc-proof/`.
The complete unchanged-object census moves from 1,092/1,852 to
1,093/1,852 EXACT; the sole gain is the full mangled symbol
`_ZN21V90ConstellationPower20getConstellationInfoEP16V90MappingParams26V90TxPowerMeasurementPointj`.
No existing EXACT symbol is lost. UNRESOLVED moves from four to three;
REGALLOC remains 29. Relative to `426814ea`, the two proof extensions together
gain precisely this function and `RxNextStateV17`, with no losses. All 300
local object hashes still match the frozen manifest after the controls and
census. `corridor-after.json` records this second result; `after.json` records
the SETcc-only result.

Larger fixed frames and nonadjacent taken-edge guards remain outside this
bounded extension. Strict whole-object equality remains outstanding.
