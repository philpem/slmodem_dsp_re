# Register SETcc jump-table proof

The bounded table prover now establishes `RxNextStateV17` identity on the
unchanged period objects at `426814ea`. The complete census moves from
1,091/1,852 to 1,092/1,852 EXACT: `RxNextStateV17` is the sole gain and there
are no losses. UNRESOLVED moves from five to four; REGALLOC remains 29.
This completes an identity proof for existing code; no reconstruction source,
compiler flags, or object bytes changed.

## Supported form and safety

The pushed-frame checker accepts only an unprefixed three-byte i386 SETcc
encoding: `0f 90..9f`, ModRM register mode with extension zero, and the exact
corresponding byte-register operand. These instructions preserve flags and
write only the selected register byte. Existing alias tracking maps BL/BH to
EBX, requires EBX to have an entry save, and invalidates restoration after a
later byte write. All memory destinations remain unsupported, including the
outgoing area, saved slots, and return-address slot. Prefixes, unsupported
encodings, and overlapping relocations remain refusals.

Guard dominance, incoming edges, instruction boundaries, relocation ownership,
table extent, and ordered destination identity retain their existing checks.
Both real objects prove seven destinations at offsets 106, 151, 227, 279,
320, 385, and 437, with 168 reachable instructions, eight returns, eleven
direct ABI calls, and an eight-byte outgoing frame. Shared `body`/`verdict`
comparison reports EXACT for the real pair.

## Validation

Run the new controls with `TC_OUT` naming the period object directory:

```sh
TC_OUT=build/tc_repro python3 tools/toolchain/jumptable_setcc_controls.py
```

The controls exercise all 16 conditions across eight byte registers: 128
positive identical-pair EXACT decisions. Twelve unsafe or unsupported cases
are refused and remain UNRESOLVED through the shared verdict, including
saved/return/outgoing memory writes, prefixes, a nonzero ModRM extension,
BL/BH writes after restoration, guard bypass, and an overlapping relocation.
A changed ordered table produces RELOC. The script also reruns the inherited
35 ABI controls (three accepted, 32 refused).

The existing table and integration controls passed, as did all 28 fixed-frame
controls (three accepted, 25 refused, two real pairs) and the comparator's
59 relocation self-tests. No runtime differential was needed for this
apparatus-only change. The parent integration owns `make phase`.

## Census provenance

Artifacts are under `build/setcc-proof/` in the isolated
`recovery-unblock-proof` worktree: `before.json`, `after.json`, object manifest,
and control logs. The reference is the repository's `ref/slmodemd/dsplibs.o`.
The frozen 300-object input is the parent worktree's
`build/recovery-unblock/baseline`, verified against its `baseline-objects.json`;
that manifest's SHA256 is
`fd20dbfa8802f6f3007b0caed051190783a41402960150b2fbc0410cf7d8397a`.
All 300 object hashes match.

The new worktree's checkout timestamps initially triggered the comparator's
staleness refusal. Both censuses therefore used a local byte-identical copy
with fresh copy timestamps, after hash verification. The baseline census
loaded the `426814ea` prover into the production comparator; the candidate
used the modified prover. No staleness check or comparison rule was disabled.
The original rejected runs provide no census result.

The strict whole-object gate remains outstanding. The separate
`V90ConstellationPower::getConstellationInfo` guard/frame proof is not covered
by this result.
