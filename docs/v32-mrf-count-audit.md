# V32 unsigned MRF count boundary

The native `DemodDataV32` zero-extends its unsigned-word formal at `0x81c0d`
and publishes that full word to the MRF argument slot at `0x81c1a`. The source's
`(short)count` instead emits MOVSWL. Removing that cast restores the original
outgoing value boundary; the int MRF formal and its signed consumption inside
the callee are unchanged.

This review replays only the baseline and count endpoint from
`tools/gcc3_boundary_transfer.py` (historical pin `ec575202`). It does not reopen
local declaration, register, source, or compiler-option palettes.

## Reproduction

The replay pins source to `6b16d64d90891138f9891067bb3e41cad136853d`, uses the shared
Gentoo toolchain helper and appends `DSPLIB_REPRODUCE_BUGS`. Frozen object/config
hashes and the source hash are in `build/v32-mrf-count293/manifest.json`.
The source matches `96ce7c20d`, and the archived `build/copy293-baseline` object
raw-matches the frozen object. The archive initializes new frozen inputs;
subsequent runs never overwrite them with current production objects.

```sh
python3 tools/recovery_v32_mrf_count.py
python3 tools/recovery_v32_mrf_count.py --audit
python3 tools/recovery_v32_mrf_count.py --trace
python3 tools/recovery_v32_mrf_count.py --period
```

The complete count-cell compiler invocation is:

```sh
cd /work/V32int/count && export PATH=/usr/i386-pc-linux-gnu/gcc-bin/3.4:$PATH; exec gcc -c -O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -I/src/include -D__SIZEOF_POINTER__=4 -include /src/tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS -DDSPLIB_REPRODUCE_BUGS -v -save-temps -da -dP -DDSPLIB_REPRODUCE_BUGS -o /work/V32int/count/candidate.o /work/V32int/count/V32int.c
```

`results.json` records the full Docker argv, image/configuration, source/header
and object hashes, compiler and assembler identities, and every function verdict.
No host-generated compiler dump participates.

## Complete-TU collateral

| Function | Native bytes | Both cells' bytes | Baseline grade | Count grade |
|---|---:|---:|---|---|
| DemodDataV32 | 604 | 600 | SIZE 4 | SIZE 4 |
| SetAdaptEcV32 | 319 | 289 | SIZE 30 | SIZE 30 |
| SetAdaptEqV32 | 79 | 79 | EXACT | BYTES 7 |
| SetECRndTripDelayV32 | 233 | 228 | SIZE 5 | SIZE 5 |

All 24 bodies are audited, including non-exact methods: four change and twenty
remain unchanged. Exact names decrease from 14 to 13, with the SetAdaptEqV32 loss
shown explicitly. Data, BSS, bindings and resolved relocation consumers are equal.
The boundary detector accepts native and count MOVZWL/full-word publication and
rejects the signed baseline as its known negative control.

## First divergence in the unchanged collateral

All three adaptation/delay methods have identical source. Thirty saved RTL
streams per method were compared. Twenty-three streams, expansion through
`27.flow2`, agree after removing only artifact directory names and compiler process
addresses in block/declaration notes. UIDs, registers, modes and constants remain
part of the comparison. The first difference is `28.peephole2` in all three:

- SetAdaptEqV32 selects DX instead of CX for the constant-one word store and
  swaps the corresponding zero scratch. Its final body differs at seven register
  operands; its mnemonic sequence, length and branch structure agree.
- SetAdaptEcV32 chooses different scratch registers for the two zero stores.
- SetECRndTripDelayV32 chooses CX instead of BX for an unused stack-pop destination.

`trace.json` records every stream hash and the seven final Eq operand changes;
the three `*-first-stage.diff` files preserve the actual first divergences.
This locates compiler-state collateral after identical allocation/reload/flow
inputs. It does not recover the original compiler history or justify forcing a
scratch register to protect an exact match.

## Period boundary and anchor maintenance

Both cells pass all three existing fixtures: `t_v32demod` 19,701 checks,
`t_v32fpsub` 798, and `t_v32fpctl` 6,546, totaling 27,045 checks per cell.
The latter fixtures exercise the affected adaptation and round-trip methods.
These are existing component boundaries; no huge-count or unsafe buffer fixture
was added, and no high-count modem reachability claim follows from the MOVZWL.

The existing wrong-resampler-owner mutation needs its exact call anchor changed
from `(short)count` to `count` on both sides. The wrong owner remains unchanged.
`proposed-mutation-row.json` records one old and one candidate match. No mutant
execution or campaign is added, following the user's instruction.

Production now contains the single cast removal and mechanical anchor update.
The focused production run passes three fixtures and 27,045 checks; its object is
raw-identical to the audited count candidate (`production-audit.json`). Default
replay also succeeds after source adoption using the frozen baseline. Main source
is frozen for parent integration, which owns the full reconstruction gate and
records the intentional exact loss.


## CI review registry

The original source commit recorded the deliberate exact loss and updated the
local exact baseline, but omitted CI's separate reviewed-loss registry. The
registry now pins this accepted result to master base `a87ae5c35` and body plus
relocation fingerprint
`f3bb17f9459ca088364df6b8747c39e416ee5bef4ad70995714377c201e98fd5`.
The archived source/evidence commit `dfc23b7d1` remains available after rebase.
The first master update changed no reconstruction inputs; PR295 then added
an INFO0 carrier fix guarded out of bug-reproduction builds. It does not
change this V32 TU or its reviewed fingerprint. Comparing the retained base census to the current canonical census
rejects the missing review, then accepts the two documented losses with the
registry complete. The metric still reports both losses numerically; it does
not raise the exact count or relax period differential checks.
