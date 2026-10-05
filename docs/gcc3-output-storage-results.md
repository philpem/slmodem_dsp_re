# Output storage explains a second exact reciprocal

Base `f7b95e35`, merged #269. PR #263 observed at `97e8a10c`; its V34
headers/sources, Makefile, census and mutation snapshots are excluded. Dirty
main checkout untouched. All new artifacts live on ZFS in an isolated worktree.
No #22 edits, fuzzing or mutation execution.

## Measured explanation

Original FPM_div_32 places its count at esp+0x10 and mantissa at+0x12. The
previous pointed-counter control places them at+0x12/+0x10 respectively,
leaving 148 bytes versus the original147. Initial01.rtl already assigns
mantissa=-4/count=-2 in that control. The exact 16-bit sibling instead has
mantissa=-2/count=-4, matching the original output-address roles in both
reciprocals. Source output declaration boundaries account for those initial
allocations; this is not a late allocator or register-name search.

The machine description explains the extra byte. GCC3's `*movhi_1/3` can emit
MOVL for an aligned memory address, or MOVZWL for an unaligned one. Installed
`-dP` assembly proves the final count load is **HI memory to an HI register**
in both cases, despite MOVL using a32-bit physical load. The unchanged UID90
moves from esp+18/MOVZWL to esp+16/MOVL. Source word width stays16bits. The
memory annotation is even A8 in both final dumps: the address predicate, not
that annotation alone, controls this selection. Original DWORD loading does
not establish an int count or require a forced wide dereference.

[Official GCC3 i386.md](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.md)
contains the movhi type/mode selection;
[aligned_operand](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.c#L4274)
checks base/index alignment and displacement modulo4.
[stmt.c expand_decl](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/stmt.c#L3895)
and [function.c stack allocation](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/function.c#L516)
explain how automatic storage is assigned. Point-release source downloads
have URL/hash provenance in build/gcc3-output-storage-source/inputs.json;
actual installed Gentoo dumps decide the measurement. We did not simulate
its allocator or claim the downloaded files are its complete patch stack.

Five declared full-div32 controls cross the recovered pointed recurrence with
the sibling-supported output declaration boundary. Bare declaration movement
is raw-inert on the retained non-addressed scalar body. With the helper, both
separate and grouped mantissa/count declarations yield **EXACT147** and the
same raw object. The separate declarations are adopted for consistency with
FPM_div. No padding, alignment attribute, register constraint, helper parameter
permutation, type widening, ABI/export change or behavior deletion.

## Transfers and limits

| Family | Cells | Common verdicts | Emitted bodies | Outcome |
| --- | ---: | ---: | ---: | --- |
| div32 |5|10|10| one distinct exact gain, two raw-equal matching spellings |
| sqrt_dp |3|6|15| matching live body; one dead-pop byte differs |
| log10 |4|4|20| result-owner spellings raw-equal to pointed control; no gain |

All **12 cells /20 common verdicts /45 emitted bodies** reproduce their raw
baseline and pass complete metadata, binding/import/export, allocated nontext,
BSS and canonical nontext-relocation audits. Only the respective target bodies
change; no gains are counted twice, and no exact losses occur.

Sqrt_dp's same output-role boundary restores its complete143-byte shape and
all live operands. The only strict difference is the dead epilogue POP EDX
versus original POP ECX. UID171 appears in28.peephole2 with DX and is unchanged
through30.rnreg; this residual is not caused by late register renaming in the
measured candidate. The prior private-output control was141bytes; no
scratch/register/declaration fitting follows this near-hit, and production
sqrt stays unchanged. Do not remove FPM_sqrt's separate bounds behavior to
make its preceding body resemble the original.

Log10 already had the correct output homes and aligned load. Short/int local
table-result owners followed by compound subtraction produce the same body
as the prior205-byte pointed control (original203). That family is closed;
its remaining value assignment/scheduling is not solved by a local result.

The new trace tool reads initial output homes and matches annotated machine
selection UIDs against final RTL modes/addresses. Five controls cover aligned
and unaligned HI positives and SI/address/UID refusals. It reports explicit
denominators and does not infer source identity or alias safety.

## Replay and integration

See [predeclared domains](gcc3-output-storage-domain.md). Copy all300 baseline
objects and `.build-config` to build/production-before. The generators pin
f7b95e35, recover its source from git, and use the shared experiment toolchain
with reproduce-bugs last. Full compiler/assembler commands and identities,
source/header hashes, objects, diagnostic dumps and strict verdicts are retained.

```sh
python3 tools/gcc3_output_storage_reproduce.py --domain docs/gcc3-output-storage-domain.md --baseline-dir build/production-before
python3 tools/gcc3_output_storage_sqrt_reproduce.py --domain docs/gcc3-output-storage-domain.md --baseline-dir build/production-before
python3 tools/gcc3_output_storage_log_reproduce.py --domain docs/gcc3-output-storage-domain.md --baseline-dir build/production-before
python3 tools/gcc3_output_storage_trace.py --self-test
python3 tools/gcc3_output_storage_audit.py
python3 tools/toolchain/byteident.py --json-out build/output-storage-final-byteident.json --limit 0
python3 tools/gcc3_output_storage_production_audit.py
```

Production expected and audited delta: **1056→1057 /1852**, **114695→114842
exact original bytes**, one gain and zero losses. Only src_dsp_fpm_div32.c.o
changes; the independent winner reproduces raw and299other objects plus the
complete build configuration remain raw-equal. The final period/structural
gate is reported in F11820.

The sqrt dead scratch belongs to peephole2 in this candidate, not late
renaming. Revisit only with an independently supported preceding-TU emission
change and the existing scratch-selector diagnostic; do not fit scratch seeds
or add dead source carriers. The output-order domain is closed. Log10 needs a first-stage value/coalescing witness
before further source spellings. V8 remainder publication remains a separate
small source-boundary lead outside #263.
