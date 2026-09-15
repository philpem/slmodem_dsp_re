# Retained host-type correction

This report supersedes the NOT RUN / no-adoption status of the earlier
`dp-param-alias-experiment.md` and `dp-param-shared-header.md` experiments.
Those reports describe their isolated experimental stages, not the final gate.

## Retained change

Restore `dsp_info.clock_deviation` to the original host header's `long` in
`include/dsplib/modem_params.h`. Keep the library-internal destination field
`_tagModemParameters.clockDeviation` unchanged. No statement reordering,
attributes, casts or compiler-profile changes were retained.

The original Smart Link declaration is independent provenance for this change.
The controlled int/long and strict-aliasing experiments explain why the earlier
equal-width substitution was not code-generation-neutral. The shared-header
audit covered 26 dependent TUs: all unchanged controls were byte-identical,
and the type correction changed only `src/core/dp_param.c`.

## Final gate and independent measurements

The source tree was rebuilt after applying the actual header correction, with
the published Gentoo GCC 3.4.2-r2 image explicitly selected for compilation
and partial linking. The final `dp_param` object was independently compared
byte for byte with the successful isolated source-long/destination-int control.

```sh
make partial-link phase J=6 \
  TC_IMAGE=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest \
  PERIOD_IMG=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest
BLOB=ref/slmodemd/dsplibs.o TC_OUT=build/tc_repro \
  tools/toolchain/byteident.py --ratchet \
  --json-out build/dp-param-retained/byteident.json
tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
  build/partial/dsplibs.o --json build/dp-param-retained/partial.json
```

Reproduction build: **275 objects from 275 sources, zero failed**.
Period differential: **375 passed, zero failed**.
Phase boundary: **period differential and structural checks all OK**.
No modern portability run is claimed.

| Metric | Baseline | Retained |
| --- | ---: | ---: |
| Exact functions / 1,852 | 830 | 830 |
| Exact function bytes / 720,125 | 80,030 | 80,030 |
| `dp_runtime_create` differing bytes / 298 | 53 | 28 |
| Positioned bytes / 943,398 | 68,456 | 68,457 |
| Exact relocation records / 18,317 | 974 | 974 |
| Exact symbol records / 2,907 | 299 | 299 |
| Candidate `.text` bytes | 684,472 | 684,472 |
| Aggregate content-size deficit | 45,764 | 45,764 |

The independently rescored exact-member sets have **zero losses and zero
gains** across all 830 baseline members, not merely the stored 810-member
ratchet floor. Function buckets remain 830 EXACT, 4 UNRESOLVED, 48 REGALLOC,
80 BYTES and 890 SIZE. The canonical partial object remains **DIFFERENT**.
Symbol, binding, relocation and section-record inventories are unchanged.

The small whole-section gain and larger function-local gain are different
measurements: absolute section positioning still contains unrelated layout
differences. Neither number should be substituted for the other.

Final local logs and JSON are in `build/dp-param-retained/`. The earlier
source-order partial-link control is explicitly invalid as a comparison to
the canonical recovered-order baseline and is not used in this result.

## Guide and remaining work

`docs/method/refinement.md` now explains equal-width alias types, original
declaration evidence, crossed controls, shared-header scope and acceptance.
It explicitly does not claim that restoring this one type makes the complete
function exact. Its remaining 28 differing byte positions need an independent
source/scheduling explanation; the tested int/long family is closed.

Issue #15 already tracks replacement of the hand-copied modem ABI declarations
with vendored originals. Native LP64 `long` changes the reconstructed
`dsp_info` layout from the prior four-word view to the native host layout;
that boundary needs explicit host/library agreement in the portability work.
This is a known type/layout consequence, not a measured modern-compiler test
failure. No source workaround was added to hide it.

Issue #22 remains the broader source-versus-compiler-profile investigation.
Neither issue is closed by this bounded correction.
