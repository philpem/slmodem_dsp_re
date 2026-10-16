# CP/MP fixed-region transfer

Recovered the original fixed CP payload regions and MP CRC/Type/publication
boundaries. No whole exact function is added or lost: the exact set remains
1086/1852 symbols. All 32 finite full-TU controls pass 504 body audits; focused
period tests pass 4/4 and final phase passes 388/388. The retained source is
a partial fidelity recovery; the closure, collateral and next discriminator
are recorded below.

Baseline ea95b278, following the phase-validated DIL recovery. The complete
Gentoo production profile and final bug-reproduction define remain fixed.
This declares eight full-TU cells before compilation: four for V90CP.cpp and
four for V90MP.cpp. All unchanged baselines must reproduce canonical objects
raw; every emitted body, export, binding, named/allocated data and relocation
is audited. No score is a source-recovery acceptance criterion on its own.

V90CP::infoToBits is 2785 original bytes versus 1916 retained. Its word_18 pair
loop remains in the original (0x5266d/0x52691, backedge0x526c0), so it is
explicitly excluded from expansion. Original short_58 payloads instead have
four independent variable-count regions at 0x5277d,0x527d5,0x52835,0x52895.
Six buffer payloads likewise have separate fixed-owner regions at 0x52a69,
0x52ad8,0x52b4f,0x52bba,0x52c28,0x52ca3. Retained source rolls these outer
four/six-owner loops. Cross their fixed-region expansion independently,
preserving each inner count, source capture, framing publication, field
sequence, arithmetic and every original bug. Do not expand adjacent loops.

V90MP::bitsToInfo is 2597 original bytes versus 1710 retained. Original first
CRC validation pre-caches all 16 bytes at 0x204c1..0x2056d and shifts them in a
straight-line bit loop at 0x20580..0x20644. The second validation repeats that
graph. Retained decoder emits two separate15-stage shift backedges. Its
encoder infoToBits has the same rolled shift grammar and the original fixed
shift graph. Cross fixed encoder shifts with fixed decoder shifts, retaining
the existing late feedback mask, all tap updates, frame/count and status
logic. No helper factoring, type, flag or arbitrary source-order axes.

The historical batch100-v90-crc-shift domain changed standalone calcCRC and
evaluateCRC, using the encoder/decoder's existing grammar as its control.
It did not test fixed shifts in these encoder/decoder bodies. Feedback-mask
placement remains closed; no such experiment is repeated here.

Prediction: CP expansion produces four literal short_58 regions and six
independent buffer-owner regions; MP expansion removes the inner shift
backedges and restores original byte-state promotion. Verify those features
against the originals independently of scores. If the intended graph is
absent, identify the compiler boundary before opening more source controls.
An exact cell establishes only this finite source family; a partial cell
requires original operand/read-age evidence and differential validation.

## Initial result and declared operand-boundary cross

All 8 cells reproduce both baselines and pass metadata/data/relocation audits
over 124 shared-body verdicts. CP's two expansions together change SIZE869
to SIZE85. MP's fixed decoder changes SIZE887 to SIZE20; fixed encoder changes
SIZE1046 to SIZE539. There are zero exact gains/losses. All switch relocation
entries and owners remain visible: unchanged-body target offsets are canonical
within their functions; shifted targets inside rewritten bodies retain both
addends and unchanged source arms, without an equivalence claim from that fact.

Declare 8 further cells, four per sibling, starting from both opening axes.
CP crosses original paired-word ages with buffer-word publication ages.
For both word_18 and nof_buf pairs, the original loads low after clearing the
framing bit and high only after emitting low (0x5266d/0x52691 and
0x529c8/0x529f1). Retained source snapshots both before framing writes.
For the six buffer payloads, the original loads a word after framing and
bitIndex publication (0x52a5c→0x52a69 etc); retained source reads before them.
Move only those original reads, keeping pointer/count capture and field order.

MP crosses encoder and decoder feedback-interleaving graphs independently.
The original bit loop updates each tap among its fixed stage shifts, and the
existing standalone calcCRC already uses exactly that source grammar.
Replace the fixed fifteen moves plus late tap stores with the equivalent
interleaved stage3/stage10 expressions; retain the late byte feedback mask.
These are witnessed arithmetic/owner boundaries, not declaration or register
permutations. Measure both axes and complete bystanders before adoption.

The second8-cell cross passes 124 more body grades with no exact gain/loss.
CP's buffer read alone reaches SIZE21; restoring both original pair and buffer
ages yields SIZE69. MP interleaving leaves decoder SIZE12 and encoder SIZE523.
Close those domains without choosing a source from their sizes.

Declare 4 further full-MP cells crossing two independent original check-loop
witnesses. Original 0x20710..0x20713 uses CDQ/XOR/SUB for integer absolute
difference; retained `d < 0 ? -d : d` instead has sign branches. Test standard
`abs(d)` with its standard header, preserving byte accumulation and every
comparison. Original check-loop backedge0x2071c is JBE, while retained source
reuses signed k; test the already declared unsigned i for both sixteen-stage
comparison loops, leaving signed CRC initialization k unchanged. The source
byte differences are bounded to -255..255, so abs has no INT_MIN ambiguity here.
Prediction: native absolute-value graph and unsigned backedge recover
independently. All shared bodies, data, binding and relocations remain in the
audit, including any source-only collateral. No adjacent index/type sweep.

The 4 check cells pass64 body grades, no exact gain/loss. `abs` restores the
original CDQ/XOR/SUB graph and leaves SIZE1; the unsigned-check counter source
change is raw-inert. A one-byte size gap does not establish a near-exact body.

Declare 4 end-carrier cells from abs plus unsigned check index. Original
0x204ab ANDs a full register with 0xffffff9a and 0x204ae forms its full-width
end bound; retained byte end uses byte AND/SUB and repeated normalization.
Use unsigned int end, matching its unsigned loop consumer and the existing
standalone calcCRC contract. Independently, original reads type after the
sixteen reset stores (0x2049f after0x2049a), while retained source selects end
before reset. Cross full-width end with that post-reset read boundary. Keep
the second-pass cached end, type byte semantics and every bound value.
Prediction: full-width selection and original reset→type-read order appear
independently. No other local width or declaration choices are tested.

The 4 end cells pass64 body grades. Full-width selection restores the original
carrier and yields SIZE49; moving the read after reset is raw-inert. No exact
gain/loss. The smaller SIZE1 cell had hidden the byte selector discrepancy.

Declare 4 encoder-topology cells from the original-width/read-age decoder
control. Original infoToBits has two CRC bodies, one after each Type arm:
the long arm pre-caches CRC at 0x1fbdc; the short arm resets again at 0x1fe70
and pre-caches at 0x1fe7f. Each has its own output/padding tail. Retained source
shares one CRC body and later branches again for padding. Cross branch-local
CRC/output/padding with full-width encoder end (original AND/LEA at 0x1fbc6).
Preserve the repeated `t` selection, count/field reads and all arithmetic;
the short arm still overwrites its CRC from 0x45. No guessed helper call,
profile adjustment or corrected bug is included. Prediction: two independent
native CRC regions with the original per-arm tails, before interpreting size.

The encoder-topology4 cells pass64 body grades with no exact gain/loss.
Branch-local CRC restores two original bodies, leaving SIZE69 with byte end
and SIZE47 with the witnessed full-width end. Decoder remains SIZE49.

Declare a final4-cell encoder cross, starting from branch-local/full-width.
Original Type acquisition at 0x1f9ab follows the seventeen preamble stores
and precedes the framing zero. Cross that read age with absolute CRC output
indices: long0xab..0xba and short0x45..0x54, witnessed at 0x1fdfb..0x1fe10
and the corresponding short tail. Retain Type selection for CRC calculation,
the short-tail overwrite, full-width end and all field reads. These are the
last independent observed boundaries in this finite family; no nearby type,
local order or loop-spelling controls are authorised by a remaining score.

## Finite-domain closure

All 32 declared full-TU cells are valid, with 504 shared-body verdicts,
zero exact gains and zero exact losses. Every chained baseline raw-reproduces
its preceding selected cell. `build/recovery-bitpacker-integration/experiment-ledger.json`
records the denominator; each domain retains its complete compiler commands,
source/header hashes, selected assembler version, losing controls and audit.
The production profile stays fixed; every compile appends the bug define.

The retained source follows the complete original-shape witnesses, with
CP `axis-1-1` from boundaries and MP `late-1-absolute-1` from publication:

| Member | Original bytes | Before | Retained | Whole-body verdict |
| --- | ---: | ---: | ---: | --- |
| V90CP::infoToBits | 2785 | 1916 | 2716 | SIZE69 |
| V90MP::infoToBits | 1948 | 902 | 1933 | SIZE15 |
| V90MP::bitsToInfo | 2597 | 1710 | 2548 | SIZE49 |

These are partial source recoveries, not whole exact functions. CP's SIZE21
control omits the original paired-word read boundaries; the retained SIZE69
restores them. MP decoder's SIZE1 control still uses byte-width end selection
where the original has a full-width AND/LEA; the retained SIZE49 restores that
carrier. Encoder Type capture after preamble and absolute CRC publication
each improve SIZE47 to SIZE31; together they yield SIZE15. Their acceptance
rests on the original producers and publication graph, not those gaps.

Close fixed region topology, CRC stage interleaving, abs idiom, comparison
index signedness, end width/read age, encoder branch ownership, Type capture
and CRC publication within these declared families. The unsigned comparison
index and decoder post-reset Type acquisition are raw-inert controls, retained
because their source boundaries agree with the original. Do not repeat
nearby type, declaration-order or loop-spelling controls from the size gaps.

The next discriminator is compiler-stage evidence for a distinct remaining
producer/lifetime boundary. Original rateMask bit publication at 0x1fa47 uses
TEST/SETNE; retained source already expresses a boolean comparison but emits
AND/MOV. Trace where that comparison is folded in the saved RTL before
proposing another expression or profile control. The selected MP encoder already has the original
36-byte frame and fixed CRC graphs, but captures Type at stack+0x23 where the
original uses+0x21, and schedules the framing zero after publishing Type where
the original places it before. Trace that carrier and store through the saved
RTL/combine/global-allocation dumps before proposing any new source axis.
A stack-slot difference alone does not license a declaration permutation.
CP and the decoder require the same operand/read-age audit of their remaining
structural regions before another finite source domain. The saved bbalign
reports are locators; they do not establish root cause or source equivalence.

## Validation and collateral

Focused period validation passes 4/4 binaries: t_v90cpinfo, t_v90cpcrc,
t_v90cpb2i and t_v90mp. Six CP/MP mutation suites retain228 locators with
zero missing/nonunique, vacuous or wrong-arm anchors. Thirteen MP locators
follow the recovered grammar while preserving their existing effects; the
formerly shared encoder mutations still affect both Type arms. The decoder's
second-pass tap mutation preserves its existing measured store effect. No
mutation campaign, fuzz execution, fixture change or tolerance change occurs.
The locator audit is `build/recovery-bitpacker-integration/locator-audit.json`.

The selected objects raw-match the independent full-TU cells and the focused
period objects. Exactly2/300 production reproduction objects changed, with
298 unaffected raw hashes unchanged. The complete exact-name set remains
1086/1852 shared symbols,120322 exact original bytes: zero gained and zero lost
whole exact functions. `build/recovery-bitpacker-integration/integration.json`
records both before/after hashes and raw reproduction checks. Selected SHA256:

- V90CP: `8bb7d6d3ec5326a256588f2a80fccc001f8e87b9e62693929e2ac1eee4137f8b`
- V90MP: `5abeb552e32bcad8abb0d4dadf019a94c23dbb4cd72bcb30ec5906e6c7666291`

The bbalign denominators are691→689 CP encoder,460→460 MP encoder and
571→566 MP decoder non-padding instructions. Mnemonic alignment still finds
59,13 and 28 structural groups respectively; equal counts do not mean identical
operands or control flow. Logs live beside integration.json. The original and
candidate switch relocation entries remain individually recorded by all six
domain audits; rewritten-owner target offsets are visible rather than excluded.

A first complete phase run passes 388/0 and all structural checks. A final
phase run covers whitespace cleanup and the final locator prose as well.
The first log is retained as `build/recovery-bitpacker-phase-precleanup.log`;
its verdict is not substituted for the final run.

Full-TU collateral is not inferred from the unchanged exact set. All 14
source-unchanged CP bodies have identical raw body/relocation records; two
switch-bearing bodies remain UNRESOLVED to this checkout's comparator, and
their switch entries/addends are separately audited. Of14 source-unchanged
MP bodies,13 are raw-identical and evaluateCRC changes18 bytes at equal size
after branch-local encoder expansion. Its unchanged source and both complete
instruction lists are retained in integration.json. The observed differences
include byte-load/store register substitutions; a single global register
renaming does not prove equivalence, and no compiler root cause is claimed.
The focused t_v90mp fixture explicitly checks evaluateCRC's return and complete
object against the blob. Its existing nonexact verdict remains nonexact; no
exact body is lost. This collateral is part of the retained full-TU result.

Final `make phase J=3`: exit 0, period differential 388 passed,0 failed,
all structural/provenance checks pass. Both final period objects postdate
their sources and raw-match the selected native/candidate objects. The final
log is `build/recovery-bitpacker-phase.log`; integration.json records the
final verdict separately from the first run. evaluateCRC has 162 instructions
on each side and 18 operand-only changed rows, with both listings retained.
