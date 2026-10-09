# Value-timing and compiler-stage follow-up to PR280

Baseline is merged master fa941457, with 1074/1852 strict exact functions and
118087 exact reference bytes. PR280's deciding period gate passed388/0; both
GitHub checks passed before its merge. This pass changes analysis apparatus and
documentation, not reconstruction source. No fuzzing/mutation cases executed.

## Wider binary screen

`tools/value_timing_tail_screen.py` examines204 source TUs outside fax, V34
and the shared-structure session. It maps1399 shared emitted bodies (including
repeated COMDAT definitions),879 already exact,404 nonexact originals of at
most850 bytes. One five-byte thunk is excluded from analysis.219 small bodies
have equal nonzero call counts;192 bodies have a linear call-position memory
access delta, an extra retained return, or a different count of jumps landing
on memory-load instructions. These are clues, not192 source fixes.

Memory displacements are not field identities. Registers can name different
objects, indexed and stack accesses are omitted, and call epochs count preceding
calls in assembly layout, **not** calls on an executed path. Review complete
branch targets, use lifetimes and relocation-resolved callees before proposing
source changes. The tool bounds instructions by ELF symbol size, saves object
hashes/build configuration/census hash, and retains full operand witnesses.
Two historical positives (V17TX_control and fax_class1_status) fire; their
current exact bodies clear all three detectors. This proves the screen runs,
not that its rankings identify unique source preimages.

Replay from the investigation worktree, preserving historical objects:

```sh
python3 tools/value_timing_tail_screen.py --objects build/tc_out \
  --census build/byteident-reload-batch.json \
  --known-old-objects build/reload-baseline \
  --output build/value-timing-tail-screen.json
```

Review rejected previously tested candidates rather than silently reopening
them. isV90WithEia6/getV90Capability lead back to F11663/F11665's closed
Boolean-width/capture families. TxNoCarrierV32's countdown/root/ring family
was already crossed in services_v32_silent_ring_reproduce.py. FSE_decision_trn
also includes an original empty loop deliberately omitted under F1604: extra
current returns do not uniquely identify missing shared-tail source. No
register, type or token variants are proposed for those closed families here.

## V.8 bit-reader bounded control

Fresh v8_getbit operand review nominates a captured signed-word count, separate
whole/truncated word loads, signed word indexing, logical right shifts and the
same CRC loop operations as v8_crc. The original tail after CRC processing
bypasses the nleft/shifter reloads; retained source rereads nleft. These are
observable source-shape differences, not merely different register names.

Exactly four full-V8global cells cross unchanged/source word-and-count graph
with existing CRC helper reuse. Gentoo3.4.2-r2 and its executed assembler
2.15.92.0.2 use every production flag and bug reproduction define. Untouched
raw baseline reproduces production. The signed-word graph has9 word-operand
compares in initial RTL versus4 and eliminates the two arithmetic-right-shift
patterns; the detector therefore observes the intended transformation.

| Cell | v8_getbit strict result (original453B) |
| --- | --- |
| Baseline | SIZE70 |
| CRC helper reuse | SIZE62 |
| Signed-word/captured-count graph | SIZE58 |
| Combined | SIZE78 |

These are **length gaps**, not counts of differing bytes. Four valid cells /
52 live body grades yield zero gains/losses. Only v8_getbit changes; all12
siblings, bindings, BSS, allocated nontext contents and canonical nontext
relocations agree. Later function starts move with the target length, while
sibling bodies and sizes remain fixed. No source adoption or candidate runtime
claim; a smaller gap is not a recovered function. The combined source also
changes negative-wordbits behavior and needs boundary/lifecycle evidence before
any independent semantic correction can be considered.

Replay:

```sh
python3 tools/v8_getbit_value_age_reproduce.py \
  --domain 'four cells: signed-word captured-count graph crossed with v8_crc reuse; original 0x754b0..0x75675'
python3 tools/v8_getbit_value_age_audit.py
python3 tools/gcc3_stage_divergence.py \
  build/v8-getbit-value-age/V8global/baseline \
  build/v8-getbit-value-age/V8global/word1-helper0 --function v8_getbit \
  --output build/v8-getbit-value-age/stage-divergence.json
```

Close this four-cell domain. Next evidence must isolate the remaining original
load/CRC carrier or a caller-established signed-word boundary; don't expand
nearby declarations, casts or register choices from size scores.

## Compiler-state diagnostic

The generic `gcc3_stage_divergence.py` now transfers the constructor historical
proof to arbitrary matching dump directories, explicit function headers and
clone selections. Its14 positive/refusal controls and actual V.8 stream
comparison are documented in [the diagnostic guide](gcc3-stage-divergence-diagnostic.md).
The writer trace reports static immediate-store transformations; it does not
invent dynamic scratch-search/cursor values. Later register renaming can
remove a peephole difference in one clone while preserving it in another.

## Work remaining

Strict production remains1074/1852:778 functions are not exact. This is not a
claim that all778 share one cause. The wider screen's192 clues require review
against closed domains and actual paths. Prioritize unique original operands
and use ages; keep source recovery separate from stateful compiler diagnosis.
The constructor floor remains an unresolved historically supported mismatch;
correct float typing and the floor are preserved. Original profile work stays
with the other agent on issue22; shared-structure PR263 remains untouched.

The separately owned [fax screen](fax-tail-timing-results.md) covers78TUs,
317 shared functions and97 small nonexact originals. Two baseline/candidate
pairs add4cells/20grades, no gains/losses. Together this pass has8valid full-TU
cells/72live body grades. SDM's second read survives allocation, then becomes
an AX selfcopy at postreload. The next concrete investigation is that pass's
value/address/mode knowledge and invalidations; the observed widened original
read does not alone prove a source fix. FSE's branch-local publication matches
size but remainsBYTES199. These new measured boundaries refine the remaining
work; none proves that source recovery is exhausted.

Final batch validation: `make phase` passes388/0 with all structural checks
green;14386 references/2962 finding headings,10038 static anchors with zero
unresolved/stale/detached/nonunique/wrong-arm results. All300 production object
hashes remain unchanged. Eight new Python tools parse;14/14 stage detector
controls and the72 live compilation-cell grades are independently checked.
