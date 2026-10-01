# Small period-GCC controls for the V34 handshake

Branch: `investigate/v34-polyvalue-rtl`, based on `086ec898`.
Experiment declared before compilation in [issue #22](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5928763753).
This is compiler diagnosis and object comparison. No production source was
changed; no fuzzing, mutation, or differential harness was run.

## Domain and controls

Three expressions, each cast to `short` at return:

```c
-21 * (int)k * k + 837 * k - 354
-21 * ((int)k * k) + 837 * k - 354
(int)k * (837 - 21 * (int)k) - 354
```

Cross full `V34hshak.c` / standalone `polyValue` with register renaming on/off:
12 cells, plus one repeated unchanged full-TU compile. Alternatives are
exploratory compiler inputs: reassociation is not a claim of equivalent C
signed-overflow domains, and none is adopted.

Compiler: GCC 3.4.2-r2, banner `Gentoo Linux 3.4.2-r2, ssp-3.4.1-1,
pie-8.7.6.5`; selected assembler GNU as 2.15.92.0.2 20040927.
The saved retained `.build-config` supplies all flags; the helper appends
`DSPLIB_REPRODUCE_BUGS`. The sole profile control appends
`-fno-rename-registers`. All cells request `-da`. Include paths are made
absolute because each compiler invocation runs inside its own cell directory.

The first run compiled objects but put dumps in a shared cwd. Its artifacts
remain explicitly invalid for RTL interpretation in
`build/v34-polyvalue-invalid-dump-path/`. The corrected complete rerun stores
31 dump files per cell in `build/v34-polyvalue-rtl/`.

## Measurements

| Expression | Full TU bytes, rename on/off | Blob verdict, on/off | Full vs standalone |
| --- | --- | --- | --- |
| Retained | 31 / 31 | SIZE(3) / SIZE(3) | EXACT / EXACT |
| Square first | 31 / 31 | SIZE(3) / SIZE(3) | EXACT / EXACT |
| Factored | 28 / 28 | BYTES(22) / BYTES(22) | EXACT / EXACT |

Blob `polyValue` is 28 bytes. The factored expression reaches that length
through a different computation graph; matching length is not recovery.
For retained and square-first forms the on/off function bodies are exactly
identical; the factored form changes bytes when renaming is disabled.

The repeated baseline is byte-identical across the whole object. All full-TU
cells preserve 55 global names/bindings; standalone cells preserve their one
global. The canonical `byteident.verdict` scores 29 shared full-TU functions.
All source cells have zero exact gains at their respective profiles.
Disabling renaming loses `dftRetrainDetInit` from the retained four-name exact
set, leaving three exact functions. Other nonexact bodies are recorded in the
manifest, rather than assumed unchanged from equal grade counts.

## What the RTL establishes

In retained `polyValue`, `.01.rtl` already contains a multiplication of `k*k`,
followed by shift/add synthesis of multiplication by 21, then negation.
Thus the instruction-selection distinction exists at RTL expansion, before
local/global allocation, reload and post-reload renaming. Allocation alone
cannot turn that initial graph into the blob's two multiplies and final LEA.
The standalone/full-TU byte identity validates this reduction for all six
tested source/profile combinations.

`dftRetrainDetInit` supplies the useful allocation control in the same TU:
131 bytes, EXACT against the blob with retained flags. Its `.25.greg` dumps
are identical with renaming on/off. In the on-cell `.30.rnreg`, the compiler
explicitly reports four chains renamed:

```text
Register ax in insn 145, renamed as dx
Register dx in insn 147, renamed as cx
Register cx in insn 149, renamed as ax
Register ax in insn 151, renamed as dx
```

Final assembly differs in both register choice and ordering of independent
stores to the retrain fields. Therefore this small control exposes the
renaming/scheduling interaction, not merely a global register permutation.
The blob follows the renaming-enabled result exactly. Inspect `.rnreg`,
`.bbro`, and `.sched2` before attributing the reordered stores to source order.

## Next discriminator

Keep the source fixed and explain this exact control's four renaming choices
from the period `regrename.c` decision rules and hard-register liveness.
Trace which ordering differences first appear in `.sched2` and why changed
register dependencies permit them. Then identify a short handshake tail with
the same dependency pattern and check whether this mechanism explains its
register-coloured duplication/merging. Do not transfer conclusions merely
because another function has similar length or mnemonics.

Further `polyValue` source/profile experiments require a new declared domain
aimed at the measured expansion distinction. The completed domain has no blob
preimage. Neither an arbitrary flag search nor the 28-byte factored near-match
is a reason to edit reconstruction source.

## Reproduction

```sh
python3 tools/v34_polyvalue_rtl.py \
  --config /path/to/retained/build/tc_out/.build-config \
  --blob /path/to/ref/slmodemd/dsplibs.o > /tmp/v34-polyvalue.log 2>&1
```

The artifact manifest records source/header/object hashes, revision, complete
compiler argv, exports, per-function verdicts and dump names. Focused dump
extracts include `polyValue`, `dftRetrainDetInit` and `detectRetrainReq`.
The run log records compiler and executed assembler identity. No harness
results are claimed for this apparatus-only investigation.
