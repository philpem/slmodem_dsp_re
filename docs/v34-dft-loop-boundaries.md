# V34 DFT input scope and arithmetic boundaries

Local declaration before compilation, master6981ee37 (merged PR254). Findings
F11703-F11704 reserved locally; #22 read at01:53:58 notes, PR245 paths avoided.

The prior FPM_SDM_init definition-order domain is closed by Playbook lever9a,
so it is not rerun. V34descrambler's loop-unswitching residual was already
measured; no new flag matrix. For dftenergy, GCC3 i386.h explicitly leaves
SHIFT_COUNT_TRUNCATED undefined because bit operations do not truncate counts;
combine.c defaults it to0. Its retained mask is not proof of author intent,
but removing the well-defined count mask merely to fit the blob is not tested.

## Finite dftupdate domain

Reference dftupdate200 bytes; current201. The reference advances the sample
pointer by2 per outer iteration, loads the current sample inside the bin loop
after updating phase, and interleaves real/imaginary product calculations with
their integer accumulator updates. Current source indexes samples[i], loads
once before the bin loop, and computes both products before either update.

Fully cross three independent two-valued source axes: indexed vs advancing
sample pointer; outer sample load vs per-bin load after phase/index update;
paired products vs real-product/real-update/imag-product/imag-update. Eight
complete-TU cells, including the unchanged baseline. Count and all signed-short
increments, cosine table, bin cursor, wrapping additions, x87 sum operations
and nonpositive bounds unchanged. Predict the cursor restores EBP input cursor
and ADD2, per-bin scope restores the reference load boundary and releases its
long-lived sample pseudo, and interleaving restores product/accumulator schedule.
The combined cell may be exact; falsifiers are unchanged initial sample-load
scope, unexpected source hash collisions, any sibling/data/export changes,
non-reproducing baseline or zero exact candidates. If none matches, inspect the
first remaining arithmetic/lifetime boundary; do not fit registers by spelling.

Use shared experiment_toolchain with complete saved .build-config flags,
Gentoo GCC3.4.2-r2 and executed assembler identity, bug define appended last,
-da diagnostics. Raw full-TU baseline and all300 production objects are checked.
Production adoption requires make phase; no fuzzing or mutation execution.

## First result and discriminating channel-order domain

Eight valid cells: cursor+per-bin gives reference200-byte length with49 byte
differences; adding integer product/update interleaving gives200/BYTES38.
All eight preserve cosread/dftenergy and costbl; no exact gains/losses.
The combined cell's prefix and outer-loop tail match the reference exactly.
Its first mnemonic divergence is inner row31: the blob pushes the real product
for x87 conversion before multiplying the imaginary product; ours IMUL comes
first. The source currently completes both integer channels before either
floating channel. That is an independently observable computation boundary.

Second domain before compilation: four complete TUs, original baseline;
channel-order only (real product, real integer update, real floating update,
then imaginary product, integer update, floating update), retaining original
indexed/outer-load source; prior cursor/per-bin/interleaved control; that
control with channel ordering. No arithmetic operation, rounding point, table
index or input bound changes. Predict the combined channel cell restores
PUSH/FILD placement and product lifetimes, potentially exact; the channel-only
control discriminates it from the independent input-cursor/load-scope effects.
Falsifiers: real FILD stays after imaginary IMUL in all channel cells, changed
bystander/data/binding, or no exact candidate. Stop the four-cell domain after
scoring; any next experiment requires a new first-divergence boundary.

## Recovered source and pass evidence

The cursor/per-bin/channel cell is EXACT200/200. Channel-only remains SIZE1;
input-cursor/per-bin/interleaved remains BYTES38. This is a crossed recovery:
neither the input boundary nor final channel boundary alone closes the body.
Source and combine now consume the real product into both its integer and
floating accumulators before computing the imaginary product. Neither product,
wrapping operation, cosine index, double addition nor rounding point changes.
The copy is a source preimage in the declared domain, not a unique spelling.

At combine, the negative control computes imaginary product at UID68 and
converts real product at UID74. The exact source converts real at UID63 and
computes imaginary at UID73. This reverses their lifetime overlap before
allocation and explains PUSH/FILD before imaginary IMUL, rather than fitting
ECX/EDX names. The final scheduled instructions and every register then match.
Indexed/outer-load control has a scaled samples+i*2 load before phase update;
the exact control has a direct samples load after phase update. Signed-short
loop counters and increments remain unchanged throughout.

## Complete controls and production review

Twelve valid full-TU cells, ten source hashes and ten objects; no excluded
compiler cells in this investigation. Each TU has three functions and one
512-byte cosine table. Metadata/types/binding/visibility, named data bytes and
offsets, allocated nontext and all three canonical costbl relocation targets
agree with each baseline. Only dftupdate changes; cosread stays exact and
dftenergy stays unchanged. Twenty-four selected stage records parse; twelve
positive/negative channel, address and load-scope controls pass.

Fresh300-object baseline raw-matches master's933-exact census. Production has
299/300 unchanged raw objects; DFTC.c.o raw-matches the exact candidate even
after clarifying comments. Whole-tree933→934/1852 exact,96367 exact bytes
(+200), sole gain dftupdate and zero losses. No profile/flag/header/ABI changes.

Replay with the preserved pre-adoption baseline objects/config:

```
python3 tools/gcc3_v34_dft_reproduce.py --domain docs/v34-dft-loop-boundaries.md --baseline-dir BASELINE
python3 tools/gcc3_v34_dft_reproduce.py --channel-order --domain docs/v34-dft-loop-boundaries.md --baseline-dir BASELINE
python3 tools/gcc3_v34_dft_audit.py
```

The driver reads complete historical source6981ee37, records source/header
hashes, actual compiler commands and the Gentoo compiler/executed assembler.
Artifacts: build/gcc3-v34-dft-boundaries{,-channel}/results.json and per-cell
dumps; build/dft-{full-tu-audit,stage-patterns,baseline-proof,production-review}.json.
No fuzzing or mutation execution; existing fixed differential fixtures decide.

Validation: make phase J=4 passes388/0 and all structural gates. Fixed DFT
fixture:256 table checks,256 accessor checks,27984 tone-bank checks,144496
quiet/loud/block-size checks,534 behavior checks and3 coverage checks. This
includes zero samples, zero bins and the existing long-integration wrap paths.
Full refcheck:14280 references and2808 finding headings valid. No fixture or
mutation metadata edits.

Complete300-input partial links reuse one recovered order on both sides.
Positioned equal bytes68680/943398, relocation records1026/18317 and symbol
records394/2907 stay unchanged; both verdicts DIFFERENT. Recovering this body
changes no TU extent after alignment, and does not establish the whole object's
original source partition, layout or optimization profile. Reports are
build/dft-ordered-{before,after}.json; order proof build/dft-input-order.json.

Static anchors:285 suite declarations/10038 anchors,0 detached or nonunique.
