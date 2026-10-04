# Loop memory promotion: one complete gain and bounded transfers

Base `b470429e` (merged #268), Gentoo GCC 3.4.2-r2 and the unchanged
production `.build-config`. Experiments use the shared toolchain helpers,
append `DSPLIB_REPRODUCE_BUGS` last, execute the compiler-selected assembler,
and preserve complete commands, versions, hashes, sources, dumps and verdicts.
Each package reproduces its unchanged full-TU baseline raw before comparison.
No fuzzing or mutation execution; #22 and the other session's #263 untouched.

## The new discriminator

The original reciprocal normalizer has two local word addresses and a count
loop with `LEA 1(old),next; MOV next,old`, followed by one terminal count-word
store. Earlier recovered helpers incremented a private int and published it
once, producing `INC` instead. Those negative controls did not test a writable
output word on every iteration.

`(*count)++` creates a real memory recurrence. The installed compiler's
`09.loop` dump explicitly reports `Hoisted regno ... r/w from (mem:HI ... count)`.
Its promoted shadow has one retained output store outside the loop. This
recovers the LEA/copy mechanism without specifying any physical register.
[Official GCC 3.4.2 loop.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/loop.c)
explains `load_mems` (line 9773): hoist loads and write back writable loop
memory after the loop. The downloaded point-release source is explanatory;
the installed Gentoo compiler's actual dumps establish this measurement.

FPM_div also requires count initialization inside the helper, after the
zero-denominator guard. Pointer recurrence with caller initialization is
150 bytes but BYTES46. Helper initialization yields **EXACT150**, including
all original relocation destinations. Only `src/dsp/fpm_div.c` is adopted.
The normalization of nonzero 16-bit inputs takes at most 15 shifts, so the
word count cannot wrap. Zero handling, index/table sentinel, bug compatibility,
output ordering and public ABI remain unchanged. This is an evidenced matching
source family, not proof of the author's unique spelling or a global profile.

## Declared finite domains and outcomes

| Package | Cells | Blob-common verdicts | All emitted bodies | Result |
| --- | ---: | ---: | ---: | --- |
| normalization-owner | 6 | 9 | 30 | log/sqrt terminal vs pointed output; no gain |
| normalization-loop | 10 | 15 | 50 | guarded-do vs top-tested while crossed with ownership; no gain |
| normalization-reciprocal | 6 | 9 | 15 | both reciprocal helpers; no gain |
| normalization-guard | 3 | 3 | 9 | baseline/early/helper initialization; FPM_div exact |
| normalization-v8 | 4 | 52 | 52 | V8agc ownership crossed with halving/word-index controls; no gain |
| unblock-v8-runs | 4 | 16 | 16 | signed/unsigned run consumption crossed with zero-only termination; no gain |

The first four package totals are 25 cells, 36 common verdicts and 104 emitted
bodies. Combined totals are **33 cells, 104 common verdicts, 172 emitted bodies**.
Every non-target body, symbol/import/binding record, allocated nontext byte, BSS size
and canonical nontext relocation remains unchanged. There are zero exact
losses. Diagnostic instruction recoveries are not production adoptions.

Log10's pointed form reaches 205 bytes versus original 203 and recovers the
normalization prefix; its arithmetic/conversion tail remains different.
Sqrt_dp's pointed while reaches 141 versus 143 and restores hot-loop layout,
but swaps private output slots. Div32 reaches 148 versus 147 with different
output storage and count-load width. Do not permute declarations, parameters,
slots or register spellings. FPM_sqrt's separate bound check is retained;
absence from the original does not authorize deleting behavior to get a hit.

V8agc's pointed+auxiliary control reaches 1009 versus 1126 (baseline 961),
restoring count normalization/halving/word-index topology. Its whole-body
convolution/history ownership and widths still differ. All 12 bystanders
remain unchanged. No source edits are adopted from that finite domain.

V8Dpsk is a useful screen false positive: its helper already increments a
pointed structure word. Original unsigned SHR and CMP/JBE/JA separately
justify unsigned run consumption controls. They restore those operands;
zero-only postdecrement reduces the raw size gap from 21 to 17, but introduces
DEC/CMP(-1)/JNE rather than original DEC/JNE. Original remainder publication
precedes bit pushing, while the retained helper returns it afterward. These
are new ownership/termination questions, not permission for near-size fitting.
Existing signed field-increment wrap remains an independent typing question.
No V8 production/header changes.

## Detector, census and next steps

`gcc3_loop_memory_trace.py` reads the compiler's announced promotions, shadow
uses, retained stores and loop depths. Five controls cover a writable positive,
a no-promotion negative and three refusals. Real pointed helpers each have
one HI count promotion and one output store at depth zero; the private-counter
controls have none. The tool does not infer alias safety or original source.

The operand screen checks 300 objects / 1,886 common defining copies: eleven
eligible original functions, all non-exact on the baseline. A real FPM_div
positive and a removed-copy negative validate the screen. Four V34 candidates
are reserved for the other session; the five FPM candidates and two V8
candidates are documented here. The pattern alone is not proof of a missing
memory recurrence.

The next independent discriminator is output storage/lifetime: trace source
allocations and output-address operands through expansion and stack assignment
before revisiting log/sqrt/div32. For V8, establish the original counter width
or remainder-publication boundary before testing another finite family. A
near-match by itself does not reopen the old allocation/profile domains.

## Replay

Each package requires a declared domain file. This report records the bounded
families above; replay pins the base revision and obtains its source with git,
so the integrated helper does not contaminate the baseline. Preserve a copy of
all 300 base objects in `build/production-before` and its `.build-config`.

```sh
python3 tools/gcc3_normalization_owner_reproduce.py --domain docs/gcc3-loop-memory-results.md --baseline-dir build/production-before
python3 tools/gcc3_normalization_loop_reproduce.py --domain docs/gcc3-loop-memory-results.md --baseline-dir build/production-before
python3 tools/gcc3_normalization_reciprocal_reproduce.py --domain docs/gcc3-loop-memory-results.md --baseline-dir build/production-before
python3 tools/gcc3_normalization_guard_reproduce.py --domain docs/gcc3-loop-memory-results.md --baseline-dir build/production-before
python3 tools/gcc3_normalization_v8_reproduce.py --domain docs/gcc3-loop-memory-results.md --baseline-dir build/production-before
python3 tools/unblock_v8_runs.py --domain docs/gcc3-loop-memory-results.md --baseline-dir build/production-before
python3 tools/gcc3_normalization_audit.py
python3 tools/gcc3_normalization_v8_audit.py
python3 tools/unblock_v8_runs_audit.py
python3 tools/gcc3_loop_memory_trace.py --self-test
python3 tools/gcc3_normalization_screen.py
python3 tools/toolchain/byteident.py --json-out build/normalization-final-byteident.json --limit 0
python3 tools/gcc3_normalization_production_audit.py
```

Production audit: **1055 → 1056 / 1852**, **114545 → 114695 exact original
bytes**. The winner reproduces raw, 299/300 other objects and the complete
configuration are raw-identical; metadata/data/BSS/nontext relocation audits
pass. The combined period/structural gate is recorded in F11817.
