# Silence-state and small-loop recovery, 2026-11-05

This wave recovers `_recieve_silence_state` EXACT274 and `faxvmi_write_fifo`
EXACT157. Both include independently observable source corrections at explicitly
synthetic component aliases. The final source preserves the native signed-word
countdown semantics without changing interfaces or applying a blanket signedness
conversion. IIR controls remain unselected.

The starting revision is `f312d45963e001f549649dcb646ca00510f63748`.
The [evidence record](evidence/silence-loop-recovery-20261105.json) contains
input hashes, experiment outcomes, closure and gates. Full frozen sources,
objects, dumps, commands and reports are retained under
`/mnt/zfs/dev/slmodem/scratch/silence-loop-20261105/`; integrated results are
in the validated worktree's `build/silence-loop/`. The toolchain is the unchanged
Gentoo GCC 3.4.2-r2 production configuration. Literal baselines reproduce the
frozen objects, and ordinary/dump/production selected objects agree literally.

## Silence: the earlier caller publication matters

The native handler reads DWORD `*word8` at `0x92c54` and retains it through the
160 SHORT TX clears, DWORD `*tx_count=160`, FPM_rms call and SHORT energy
publication. The current source instead reads it after those effects. Mapping
all nine arguments by stack depth confirms word8 is argument9 and tx_count
argument7. These are both int pointers; they can name one scalar.

Native FPM_rms reads the low16 count and sample words, accumulates in registers
and writes an outgoing argument on its private stack. Its only callee,
FPM_sqrt_dp, writes two private stack words and reads a table index clamped to
191. Neither writes caller memory or calls a callback. This does not validate
the late source read: the caller's preceding tx_count publication changes the
value under a same-type alias. The old source comment attributing the early
read to strict aliasing overlooked that publication; it is replaced.

| Entry snapshot | Named loop bound | Target |
| --- | --- | --- |
| no | no | SIZE1, 273 bytes |
| yes | no | BYTES2, 274 bytes |
| no | yes | SIZE1, 273 bytes |
| yes | yes | EXACT, 274 bytes |

The named bound preserves native CMP160/JL; the original direct constant
comparison became CMP159/JLE. `_send_silence_state` supplies an independently
measured same-TU example of that bound mechanism. This dimension is predicate
canonicalisation, not a signedness fix. Two additional status/state textual
reorder controls lose exactness; scheduled native store order does not establish
original textual order.

The fixture adds 16 cases / 193 assertions after the real silence initializer
with samples4, half-count2. Cases cross disjoint versus word8==tx_count,
entry values0/7, quiet/loud audio and debug levels0/2. Each history performs
one admitted state-handler call with sized buffers and guards. Expected native
state, complete context, RX/TX buffers, scalar outputs and guards are checked
alongside the differential result. The old source fails exactly four complete
fixture assertions: zero-entry aliases across both audio and debug controls.
Disjoint and nonzero-entry controls pass; the selected source passes all cases.
The aliases are C-admissible component probes, with no normal protocol history
established. The initialized state call supplies no public fax-session claim.

All 17 emitted TU definitions are compared, including 12 EXACT neighbours that
remain EXACT. Unchanged-source `fax_class1_create` retains SIZE33, with different
hard registers. `fax_class1_command` moves BYTES49/native615 to SIZE11/626.
Their complete normalised RTL is equal through flow2; peephole2 first chooses
different hard scratch registers. Command subsequently loses an inlined
send/receive-initializer shared countdown suffix at ce3, accounting for the
extra code. This is recorded collateral, not a reason to invent a source carrier.
Focused constructor/command tests and the full period gate cover the selected
source at their established fixture boundaries.

All export bindings/visibility and definition order stay fixed. Five allocated
noncode sections retain size/type/alignment. The string pool swaps existing
date/time strings; all 37 string relocations still name the same strings.
Six command switch destinations retain case identity, with three offsets
changed and three fixed; all twelve baseline/candidate destinations decode to
instruction boundaries. Pre-peephole labels and dispatch order support case
identity. Raw section identity and strict destination-offset identity are not
claimed, and this is not a general CFG equivalence proof.

## FIFO: four native properties interact

The small-loop screen inspects 300 frozen objects / 1,886 shared defining
copies. Of 1,302 native bodies of 1..250 bytes, 244 copies are inexact and
183 contain a backward intra-function jump. Backward layout also includes
shared rejoins; it is not proof of a loop. Seven symbols receive source/native
inspection; two TUs receive source matrices. The rest remain uninspected.

For faxvmi_write_fifo, native captures underrun at `0x96903` before dereferencing
the source pointer at `0x96906`. It consumes a signed-short postdecrement count,
so negative low-word values run the wrapped countdown rather than fail a
positive-count test. It acquires the framer at root+0x24 only after admission,
then captures old wr and publishes its increment at `0x9692a..31` before the
source-word load/FIFO write. The source used a pretest countdown, acquired the
framer at entry and published wr after storing the sample.

| Cell | Result |
| --- | --- |
| baseline | SIZE36 / 193 bytes |
| A: postdecrement | SIZE24 / 181 |
| B: post-guard framer | SIZE20 / 177 |
| AB | SIZE14 / 171 |
| ABC: early wr publication | BYTES22 / native157 |
| ABCD: underrun before source acquisition | EXACT157 |
| ABCD minus A / B / C / D | BYTES43 / SIZE4 / SIZE25 / BYTES22 |

The final capture-order control changes initial RTL pseudo homes for taken and
p and closes the remaining bytes. All four properties have native witnesses;
the spelling remains non-unique. Both neighbouring TU bodies, including exact
frame_reverse, retain bytes and relocations. Exports, definition order, both
allocated noncode sections and zero data-relocation fields are unchanged.

The FIFO regression adds 16 cases / 225 assertions, crossing zero/one requests,
empty/full rings, initial wr0/1 and disjoint/source-at-wr inputs. FIFO storage
is a real separate two-word array with guards. The synthetic source is the
single unsigned-short scalar `fr.wr`, requested at most once, with a legal
one-past pointer on return. Native increments wr before reading that input;
old source reads the old value. Exactly two old-source ring comparisons fail,
and the selected source passes. Complete VMI/framer state is normalised for
separate owner pointers, and buffers, guards, source extent and expected native
outputs are checked. This probe does not alias a two-word FIFO onto a scalar,
and it establishes no ordinary constructor-generated alias.

## Negative transfer and graph limits

FPM_iir_filt_II's short outer induction × short inner postdecrement produces
SIZE19/15/20/16. The native outer loop narrows its increment; the source's inner
loop already models low-word wrap explicitly. Only the target body changes in
these controls. No complete recovery or behaviour defect is established, so
this TU remains unchanged. Close these four spellings; reopen only with a new
native observation boundary or causal compiler witness. No compiler-impossibility
claim follows from the bounded negative.

TxNoCarrierV17/V27/V29 already match native unsigned count predicates. FSE_Bridge_det
has a fixed four-point search; FPM_iir_filt already uses short postdecrement.
These do not admit blind signed-consumption transfer. They received inspection,
not source edits or complete recovery claims.

Changed-TU graph screening selects seven inexact copies before and five after,
because both recovered functions leave that denominator. The separate full
class1 graph admits16/refuses1, pairs57 anchors and compares148 relations with
14 ambiguous anchors and zero findings. The explicit silence target compares
three relations, and its old positive becomes clean. FIFO pairs three anchors
but compares zero relations and refuses the stack graph: no equivalence verdict
can be drawn from its clean result. Exact bytes and period differentials supply
separate evidence. All graph reports retain parsed-input and helper hashes.

## Validation, instructions and next work

Focused silence/constructor/command tests pass4/4; FIFO pack/unframe tests pass2/2.
`make phase J=8` passes 399 period tests / zero failures and all structural
checks. The final census is 1,141 EXACT / 28 REGALLOC / 71 BYTES / 612 SIZE
out of 1,852, two EXACT gains and no protected losses.
Across the complete 300-object closure only class1 and faxvmififo change; four
of their twenty emitted bodies change. No EXACT/REGALLOC member is lost.
No mutation execution, snapshot refresh, anchor repair or modern portability
campaign ran. AGENTS now requires auditing intervening caller publications as
well as callee effects. The Playbook records matrices, ablations, collateral,
fixture domains and the IIR reopening condition.

> 🧠 **From Hindsight memory (Null datapump signed-count and V8 message graph recovery)** — the retrieved page still described the preceding null/V8 wave as unresolved. Current committed f312d4596 and its frozen evidence supersede that status; a correction was ingested. Historical memory supplied no current verdict for the new FIFO candidate.

Next investigate `faxvmi_write_frame`: its native outer postdecrement and
post-header framer acquisition at `0x969a8..0x969c2` differ from the current
pretest/entry capture. Keep frame-length capture, header publication and payload
publication distinct; FIFO's repair must not be copied wholesale. Then expand
the bounded loop screen beyond 250 bytes where the same native witnesses occur.
Keep the closed IIR and V8 spelling families parked without new evidence.
