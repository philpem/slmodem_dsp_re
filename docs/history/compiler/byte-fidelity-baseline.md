# Post-PR95 byte-fidelity baseline

Measured 2026-09-15 from merged commit `11d23fd1d7e9dc137d87874aecd0e0fd899682ba`
in a fresh isolated worktree. No source or compiler-profile changes were made.
This begins the byte-fidelity phase after issue #20's ownership/order review;
it does not promote unresolved ownership to established provenance.

## Toolchain and commands

Image: `ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`.
Recorded repository digest:
`sha256:14efe17550e390798c81d78a612b538af6c8bd70b8b6fc75ee19115147ef5d96`.
Local image ID:
`sha256:fe868cc44a48c36d1130862965729d0b384bc9e0a1908d37891aa5ae07352f16`.

Compiler: GCC 3.4.2, Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5.
The assembler selected by the compiler was executed, not merely located:
GNU assembler 2.15.92.0.2 20040927. Selected linker reports the same version.

Actual reproduction-object `.build-config`:

```text
flags -O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS
cxx   -fno-exceptions -fno-rtti -fno-math-errno -ffast-math
dcr   -O2 -fno-rerun-cse-after-loop
```

The existing DCR exception is retained, not newly justified. Period fixtures
use their existing harness flags; they are not the reproduction objects.

```sh
nohup make partial-link phase J=6 > build/byte-fidelity-baseline/gates.log 2>&1
TC_IMAGE=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest \
  tools/toolchain/partiallink.sh build/byte-fidelity-baseline/dsplibs.o \
  build/tc_repro/tc_link_manifest.txt
BLOB=ref/slmodemd/dsplibs.o TC_OUT=build/tc_repro \
  tools/toolchain/byteident.py --ratchet \
  --json-out build/byte-fidelity-baseline/byteident.json
tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
  build/byte-fidelity-baseline/dsplibs.o \
  --json build/byte-fidelity-baseline/partial.json
```

The first partial-link helper invocation selected its local Gentoo image
alias. The canonical measurement above explicitly relinks with the published
image, avoiding an assumption that the local alias is the same image.

## Results

Fresh compilation: **275 objects from 275 sources, zero failed**.
Manifest ordering: 203 ordering candidates, 72 retained in source order.
Ordering candidates are not proof of original translation-unit ownership.

Period differential: **375 passed, zero failed**.
`make phase` exited successfully with all structural checks passing.
Reference checks: 13,925 checked, zero unresolved, pending or stale.
Mutation-anchor checks: 272 suites, 10,041 mutations, zero skipped or invalid
anchors. These are structural checks, not a fresh mutation execution.
Modern portability was not run and remains separate, including issue #96.

| Function classification | Count / 1,852 shared symbols |
| --- | ---: |
| EXACT | 830 |
| UNRESOLVED relocation comparison | 4 |
| REGALLOC | 48 |
| BYTES, same size | 80 |
| SIZE | 890 |
| RELOC, differing canonical target | 0 |

Three COMDAT symbols have differing verdicts between defining objects; the
comparator scores the worst. The checked-in 810-member exact floor passes.
The complete current exact set is saved for subsequent membership comparisons;
net exact-count improvement alone is not sufficient evidence for adoption.

| Whole partial-link metric | Result |
| --- | ---: |
| Positioned reference bytes equal | 68,456 / 943,398 |
| Exact relocation records | 974 / 18,317 |
| Ordered relocation matches | 966 |
| Exact symbol records | 299 / 2,907 |
| Ordered symbol matches | 298 |
| Shared allocated-section names | 92 |
| Exact section descriptors | 67 |
| Exact section contents | 60 |
| Candidate / reference `.text` bytes | 684,472 / 728,304 |
| Candidate content-size delta | -45,764 |
| Candidate / reference NOBITS bytes | 2,840 / 2,836 |

Strict whole-object result: **DIFFERENT**. The diagnostic comparator exits
successfully when it has completed the census, not when the object is exact.
Positioned-byte matching is layout-sensitive and is separate from function
identity with canonical relocation targets.

An independent census of defined FUNC/OBJECT/NOTYPE names, including ABS,
finds matching sets of type/binding/visibility for **2,526 / 2,526 shared
names**, with 75 reference-only and 136 candidate-only names. This denominator
is broader than earlier allocated-symbol-only reports; do not compare those
counts as though they used the same filter. Agreement among shared names does
not establish a complete matching export inventory.

## Evidence and next experiments

Local machine-readable evidence is under `build/byte-fidelity-baseline/`:
`byteident.json`, `partial.json`, `bindings.json`, `object-hashes.txt`,
`image.txt`, `toolchain.txt`, and the full gate/comparison logs. The canonical
linked candidate is `dsplibs.o` in that directory. Build artifacts are not
versioned by this report.

See `v22-tone-experiment.md` for the first bounded experiment and
`byte-fidelity-tu-plan.md` for the translation-unit census and prioritized
follow-up. Follow `method/experiment-design.md` before changing source or
flags. No global optimization change, byte padding, or symbol renaming is
justified by this baseline.
