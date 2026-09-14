# Object input and definition order: September 2026 pass

This pass continues [issue #6](https://github.com/philpem/slmodem_dsp_re/issues/6)
under the open [issue #20](https://github.com/philpem/slmodem_dsp_re/issues/20).
The priority is original object input order, correct ownership, and then
function/data definition order. Compiler-profile refinement is not this pass.

## Tool control

The initial checkout was PR #92 at `b82cc2e7`, with uncommitted attribution
fixes. Those fixes were copied into an isolated worktree; the original
checkout was preserved. Shared tool commit `a1a7462b` is based on master
`f1dd519e` and contains no reconstruction-source changes.

The attribution report now preserves duplicate symbol and FILE occurrences,
keeps ambiguous candidates unresolved, and distinguishes naming inference
from ownership evidence. Partial linking generates its attribution JSON from
the current tools and blob, rather than silently using the tracked report.
The partial-link CI cache includes those dependencies.

Validation: 11/11 ELF fixture tests, 8/8 partial-comparator controls, and
`make phase`: 375 passed, 0 failed, structural checks passed. A dry-run
dependency control with `make -n -W tools/tuattrib.py
build/partial/attribution.json` schedules regeneration.

On the real blob, verification covers 23/47 local function occurrences out of
1,773 sized functions; 22/23 agree. The remaining mismatch is explicitly
`name-only`: `V34demodulate` belongs to `V34RX.c`, not the inferred `V34.c`.
This evidence class is not used to select an ordering candidate.

## Reproducible baselines

Compiler image: `ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, image ID
`sha256:fe868cc44a48c36d1130862965729d0b384bc9e0a1908d37891aa5ae07352f16`.
The executed compiler reports Gentoo GCC 3.4.2-r2; its selected assembler and
linker both report 2.15.92.0.2, dated 20040927.

Complete source profile from `.build-config`:

```text
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
-Iinclude -D__SIZEOF_POINTER__=4
-include tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS
C++: -fno-exceptions -fno-rtti -fno-math-errno -ffast-math
DCR retained exception: -O2 -fno-rerun-cse-after-loop
```

The corrected-order arms relink the *same compiled objects* as their original
arms. They isolate an ordering-policy change from a source/codegen change.

| Source / ordering tool | Positioned bytes / 943398 | Relocations / 18317 | Symbols / 2907 | Candidate NOBITS / 2836 |
| --- | ---: | ---: | ---: | ---: |
| master `f1dd519e`, original | 59872 | 918 | 232 | 2760 |
| master `f1dd519e`, corrected | 59949 | 902 | 232 | 2760 |
| PR #92 `b82cc2e7`, original | 55450 | 943 | 243 | 2804 |
| PR #92 `b82cc2e7`, corrected | 55123 | 954 | 243 | 2804 |
| DSP ownership batch, corrected | 55313 | 954 | 244 | 2804 |

All rows have 67/92 exact section descriptors. The two baseline source
revisions have the same 828/1852 strict exact functions, covering
79916/720125 reference code bytes. The reproduction define is enabled here;
the PR's published default-profile count of 824 is a different build profile.
Four unresolved relocation cases remain separate from the 828 exact matches.

Every strict partial-link verdict remains `DIFFERENT`. Positioned byte counts
are influenced by layout shifts; they do not establish ownership or justify
adding padding to the source.

## PR #92 audit and remaining inventory

The reference has 281 real FILE occurrences; PR #92 supplies 264 inputs.
Under corrected attribution, 178 inputs have ordering candidates and 86 retain
source-list order. Six inputs have conflicting symbol candidates: `faxadapt.c`,
`v17.c`, `v21.c`, `v27.c`, `v29.c`, and `V90SessionFlag.cpp`. These are an
ownership worklist, not proof of a particular split.

F11360's empty-unit conclusion is superseded by F11362: a FILE without locals
can contain globals. The `fax.c` and `NoK56Flex.cpp` corrections in PR #92
are retained. FILE sequence, global code sequence and symbol families support
them, but a shared address bracket alone does not prove global ownership.

F11361's claim that equality of complete local-name sets is a safe unique
mapping is too strong. The synthetic fixtures demonstrate identical sets in
different FILE occurrences. Even adding raw data bytes does not make every
case unique: the V17/V27/V29 receiver configuration objects share identical
local data signatures. Their mode-specific globals supply additional evidence;
the local set alone cannot distinguish those three renames.

The data-signature inventory matched 24/264 compiled inputs. It compares
local-object names, sections, sizes and raw bytes as a discriminator, not as a
substitute for relocation-aware comparison. The three selected DSP cases have
unique matching reference occurrences:

| Reconstructed input | Reference FILE sequence | Local evidence |
| --- | --- | --- |
| `fpm_mtd_cfg.c` | 247, `fpm_mtd.c` | `DEF_COEFS`, `.data`, 20 bytes at 0x81bc |
| `fpm_tone_cfg.c` | 256, `fpm_tone.c` | `ToneLPF`, `.rodata`, 106 bytes at 0xd040 |
| `vtb.c` | 258, `fpm_vtb.c` | `VTB_DIFF_TBL`, `.rodata`, 32 bytes at 0xed60 |

The first two are merged into their existing owning units; the third is
renamed. The tone configuration mutation suite follows its source. The
ownership-only cell preserves all 4916 `.text` bytes and all 32 `.rel.text`
records across the three affected units. All 259 other unchanged-path
objects remain byte-identical to the clean PR #92 build. It removes two
invented FILE records and keeps the 828 exact-function set unchanged.

## Next definition-order control

The ownership-only cell exposes an MTD `.data` ordering mismatch:
reference `FPM_MTD_CFG`, `DEF_COEFS`, `COEF_DC`; candidate `COEF_DC`,
`FPM_MTD_CFG`, `DEF_COEFS`. The bounded control changes declaration placement
only and checks actual period emission rather than assuming source order is
emission order.

The reference tone function order is create, delete, generate, generate2,
generate_demod, set_freq, set_scale, detect, find_rev, filter, kill. The
candidate emits setters and generators before create. A separate cell will
reorder complete definitions without changing their bodies, then inspect
every emitted body and relocation and re-run the exact-set and period gates.

## Deferred ownership questions

- `getbit` is LOCAL under the reference `V34hshak.c`. Its three nonrecursive
  calls are inside the reference's single `v34handshak`; the reconstruction
  carries those regions in helpers in `v34hstx1.cpp`. Restoring the local
  binding requires addressing that source split, not moving `getbit` or
  treating a globalized test object as a faithful production object.
- `v32fpdisp.c` mixes functions inferred into `V32.c` with local data whose
  actual reference owner is `V32stc.c`. Moving the whole file would not
  resolve the conflict; partition its contents first.
- There is no reference `fpm_lmsupd.c`. The contiguous LMS group follows
  `VTB_decoder`, but the intervening FILE sequence includes `fpm_adeq.c`.
  At least `fpm_vtb.c`, `fpm_adeq.c`, and `voice.c` must be considered;
  adjacency alone does not justify a merge.
- `FPM_MTD_CFG_data` remains the already-recorded invented duplicate D1101.
  Removing it changes the NULL-configuration behavior and needs its own
  differential case; moving its containing data does not resolve that defect.

Local detailed artifacts are under `/tmp/issue20-object-order-evidence/` and
`/tmp/slmodem-issue20-baseline-{f1dd519e,b82cc2e7}/`. They include full logs,
comparator JSON, exact sets and build configuration. The initial accidental
byteident invocation without `TC_OUT=build/tc_repro` was invalid and is not
used; the reproduction-profile rerun supplies the measurements above.
