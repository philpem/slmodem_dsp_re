# GCC 3 earliest-stage and store-split diagnostic

`tools/gcc3_stage_divergence.py` compares existing GCC 3 dump directories without
compiling or changing reconstruction source. It reports the earliest *observed
instruction-pattern or instruction-order* divergence for a selected exact dump
function header, with explicit selection when that header names multiple clones.

This is a diagnostic for a measured source/compiler control. Validate the raw
objects, complete compiler commands, header closure, bug define, selected
assembler and full-TU metadata first. The tool cannot establish those prerequisites
from dump text. It neither grades byte identity nor reconstructs allocator state.

## Invocation

```sh
python3 tools/gcc3_stage_divergence.py BEFORE_DUMPS AFTER_DUMPS \
  --stem V90Parameters.cpp \
  --function 'V90Parameters::V90Parameters(_tagModemParameters*)' \
  --all-clones --output constructor-stages.json
```

For a single clone, use `--clone 0`; that index is the matched-header order in
each dump, **not** a universal C1/C2 ABI identity. Verify emitted assembler order
when assigning ABI names. Default scratch inspection is `27.flow2` to
`28.peephole2`; override both window arguments for another numbered dump series.

JSON records input paths and SHA-256 hashes, stage and stream denominators,
instruction counts, changed UIDs, order equality and bounded pattern examples.
It excludes `00.cgraph` and `08.gcse`, which are not comparable complete
instruction streams. Unequal discovered stage sets, unmatched headers, ambiguous
clones, changing clone counts, duplicate instruction UIDs and truncated RTL
fail explicitly. A matching pair of incomplete dump sets can still hide an earlier
pass: "earliest" always means earliest among the reported stages.

Comparison preserves immediate values, hard and pseudo registers, modes and UID
order. Only explicit GCC tree pointer annotations are normalized; large hexadecimal
immediates remain significant. Notes, dependency/scheduler prose, block metadata
and instruction envelope fields are outside the compared pattern. Equality here
does not prove complete RTL equivalence. No register renaming is erased.

## Observable split versus scratch state

The supplementary detector recognizes a unique immediate store becoming a store
of a register to the same memory expression, with a preceding SET materializing
the same immediate into that register. It reports both store UIDs, materializer
UID, destination, value and register. This is a static shape observation using
the nearest preceding SET, **not** a CFG/liveness proof. Multiple stores to the
same expression are deliberately excluded. Calls, implicit clobbers and intervening
paths require manual review before interpreting a reported producer as causal.

Its denominator is recognized store transformations. It is not the number of
scratch searches, successful allocations, rejected candidates or cursor updates.
Every output explicitly says dynamic scratch searches/cursor values were not
measured.

The stock official GCC 3.4.2 sources explain why an earlier writer can affect a
later function's colours: `recog.c:2932` implements `peep2_find_free_register`
with a static `search_ofs`, success advances it at line 3018 and failure resets
it at line 3024. `i386.md:17507` is the SI immediate-memory split using scratch.
There is no per-function reset in this implementation. These sources explain a
possible stream-state mechanism; the actual measured compiler is Gentoo
3.4.2-r2, and no dynamic cursor trace was captured. Source hashes and compiler
provenance are in [the historical proof](v90-parameters-constructor-stage-proof.md).

## Demonstration and validation

From the integration worktree, the unchanged historical artifacts are in
`build/v90-parameters-ratchet-ab`. Read them in place:

```sh
python3 tools/gcc3_stage_divergence_validate.py \
  --controls build/v90-parameters-ratchet-ab \
  --output build/stage-divergence-validation
python3 tools/gcc3_stage_divergence.py \
  build/v90-parameters-ratchet-ab/pre-434-retype \
  build/v90-parameters-ratchet-ab/post-434-retype \
  --function 'void V90Parameters::setToDefault()' \
  --output build/stage-divergence-validation/writer.json
```

The validator passes **14/14 controls**, including historical positive/equal
controls, significant large-hex/register/order changes, notes-only equality,
tree-address normalization, parser/selection refusals and store-split positive
and negative checks. The two historical constructor comparisons each contain
29 stage pairs, two selected clones and 116 RTL streams. Pre/post first diverges
at `28.peephole2` for both clones; pre/pre has no divergence. In the positive
control clone 0 changes UIDs 116/117 at peephole2 and 116–119 at rnreg; clone 1
changes 116–119 at peephole2 but is equal again at rnreg. A first divergence
need not persist to final output.

The writer comparison contains 29 stage pairs and 58 RTL streams, first diverging
at `01.rtl`. Its static split counts are 153 pre and 152 post. The pre-only
`+0x434` observation is store UID 1246 becoming UID 2064, materialized by UID
2063 in register 4; the correctly typed post writer has no corresponding
immediate-store split. The historical audit independently established identical
final writer bytes and raw/dump-only object repeats. Identical emitted bytes
therefore do not imply identical intermediate patterns or scratch opportunities.

## Playbook conclusion ready for integration

Before interpreting a four-byte register-colour residual as local source order,
compare the measured control's matching dump stages and clone streams. If local
and global allocation agree and the first difference is peephole2, inspect prior
writers and immediate-memory scratch splits before attributing it to rnreg.
Later rnreg can propagate or eliminate an earlier difference. Preserve correct
source typing and ratchet floors; a historical exact result can compensate for
different prior TU state. Without a dynamic allocator trace, report the stateful
mechanism and static witness separately, and do not infer cursor values or tune
earlier source tokens to manufacture a colour.
