# Source C dialect controls

At source revision `13f477a9`, adding only `-std=gnu99` to the canonical
Gentoo C profile reproduces all three previously different period objects
byte for byte. All three unchanged canonical controls first reproduce the
frozen canonical objects byte for byte. No source or build-profile change is
adopted by this characterisation.

`tools/toolchain/period_inner.sh` sets `CFLAGS="$FLAGS -std=gnu99"` and uses
those flags for source C files as well as C fixtures. Its general flag banner
prints `FLAGS`, which omits that dialect option. The actual source C commands
therefore differ from the banner and from `tools/toolchain/period.mk`, whose
source C profile uses the compiler's default `gnu89` dialect. Compiler-policy
[V4](method/compilers.md) records
the harness's need for C99 syntax; it supplies no evidence for the original
library's C dialect.

## Bounded comparison

The domain contains six full-TU compilations: an unchanged canonical control
and one `-std=gnu99` control for each of the following files. Both cells use
unchanged source and headers, the complete frozen `.build-config` flags and
the shared experiment-toolchain helper's final `DSPLIB_REPRODUCE_BUGS` define.

| Translation unit | Shared emitted bodies | Shared original bodies | Changed bodies | Exact original names, both profiles |
| --- | ---: | ---: | ---: | ---: |
| `src/fax/V17t_prc.c` | 11 | 11 | 1 | 3 |
| `src/fax/class1.c` | 17 | 17 | 1 | 12 |
| `src/pump/v34/V34hshak.c` | 54 | 29 | 10 | 9 |
| Total | 82 | 57 | 12 | 24 |

There are zero exact-name gains or losses among the 57 shared original bodies.
Seventy of the 82 emitted bodies keep their canonical byte/relocation bodies.
The remaining 25 emitted names are reconstruction-only helpers. No symbols,
bindings, types or visibility change; NOBITS storage remains equal. No
additional `-Itest/harness` control is needed to reproduce the period objects.

`TxNextStateV17` changes from 1512 to 1506 bytes, with its initial stack
reservation changing from `0x13c` to `0xfc`. `fax_class1_create` changes from
1565 to 1546 bytes. Their canonical code relocation target multisets remain
equal; V17's four differing nontext jump-table destinations retain their
function owner and move within the changed body.

The ten V34 changes are `t3c_micro_moh_tone`, `t3c_micro_moh_tone_drop`,
`t41_marks_late`, `t41_micro_det_sync`, `t44_micro_det_info`, `t46_chain_full`,
`t46_micro_tx_phase1_ans`, `t53_rx_det_ab`, `t72_rx_l1` and `v34handshak`.
Under `gnu99`, `t41_micro_det_sync` has two fewer external debug-call
relocations and `t46_chain_full` has four fewer. The other ten changed bodies
across the three TUs preserve canonical code relocation target multisets.
These are static emitted-site counts, with no new claim about runtime paths.

V34's 7437-byte `.rodata.str1.4` pool changes order while preserving its
NUL-separated string multiset. Other initialised nontext contents remain
equal after relocation masking. Nontext relocation counts stay 12, 43 and
323 respectively. Changed code destinations retain their function owner
names while interior offsets move; this does not prove corresponding
semantic switch arms. Complete per-body sizes, relocation targets and ELF
audits are retained in the artifacts.

Six preprocessing controls produce byte-identical output for each source
under the two dialects. Conditional header expansion is therefore excluded
as the cause in this domain. The precise GCC lowering or optimisation
mechanism has not been isolated. This bounded comparison establishes neither
the original C dialect nor a preferred global profile.

## Replay and validation

On the unchanged `13f477a9` source/header snapshot, with its frozen canonical
object directory and period object directory available, run:

```sh
python3 tools/period_dialect_audit.py \
    --baseline-dir build/fidelity-thirteenth/baseline \
    --period-dir build/period \
    --revision 13f477a9
```

The paths are inputs; substitute the archived directories' actual locations.
Omit `--period-dir` to audit the dialect effects without requiring a separate
period object tree. The tool always recompiles the six declared cells into
its isolated output directory. It refuses source/header drift, a failed
canonical raw control or a requested period-object mismatch.

The recorded toolchain is Gentoo GCC 3.4.2-r2 from
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, targeting
`i386-pc-linux-gnu`. The assembler actually selected and executed by that
compiler is GNU assembler 2.15.92.0.2, dated 20040927. The replay records
source/header hashes, complete compiler commands and object hashes in
`build/period-dialect-audit/results.json`; `toolchain-identity.log` records the
compiler and assembler versions. `complete-audit.json`, per-TU disassembly
diffs, ELF audits and preprocessing diffs carry the fidelity checks.

The original bounded run lives in `build/v8-dialect-control`. A serializer
failure and an initial packaged-tool import failure are retained as invalid
audit attempts; the final tool replay validates the recorded conclusions.
No failing compiler cell was interpreted as an accepted result.

The V.8 fixture commit passed the current source-C period profile's focused
three-binary differential gate before this separate characterisation. This
audit does not run or replace that gate. The next recommended experiment is
bounded source/test dialect alignment, with distinct profile caches and
configuration stamps, followed by the complete period differential suite
before considering adoption. That experiment must preserve the source
profile's authority and record any compiler-dependent fixture results.
