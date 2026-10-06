# V29 epoch detector: a complete standalone body trace

At6b4509bd, the original V29RX_epoch_det is413bytes at9b670..9b80d;
retained Gentoo source emits443bytes. It has zero runtime CALL instructions
and exactly three relocations: two reads of V29RX_DEC_ANGLE and one published
V29RX_eq_train function pointer. The latter is a callback handover, not an
inlined helper. Thus there is no observed out-of-line helper boundary mismatch in this body.
Its size alone does not establish an inline-budget explanation; helpers that
were fully inlined in the original source are not ruled out by zero CALLs.

## Original control and arithmetic graph

| Region | Operations and successor |
| --- | --- |
| 9b670..9b6a4 | Capture state->cfg.owner, clear lms_on, increment decoder sym_count modulo16bits, read signed n_out and current I/Q outputs. |
| 9b6a4..9b6f2 | Narrow I1-I and Q1-Q to signed16; independently narrow I2-I0 and Q2-Q0. Square each at32bits, add/shift each pair by15 independently, add pair results and narrow final distance to signed16 promoted toSI. |
| 9b6dd..9b714 | Publish current Q and shift the two old history pairs; publish current I. Stores interleave with arithmetic in scheduled code. Six source history assignments describe the same final history. |
| 9b714..9b739 | Read *angle, narrow difference from angle_prev, publish angle_prev. Negative difference takes9b802 subtract0x8000 and narrow, then rejoins9b72f. Compare folded difference to0x4000. |
| 9b739..9b766 | Far phase: avg_far=(avg_far*31)>>5, publish intermediate; read *mag and add its signed>>5 term, publish again; select angle-table index4. |
| 9b7d4..9b7fd | Near phase: same two-store average over avg_near, select angle-table index7, join the common *angle write at9b766. |
| 9b769..9b78b | Signed train_count>128 gate. Form signed-short((far²+near²)>>15), compare distance against twice that average. False goes to final increment. |
| 9b78d..9b7b1 | Handover: mu[0]=0x14e6, lms_force=1, decision=V29RX_eq_train; decoder train_lfsr=0x55,last=0,train_count=0xffff. |
| 9b7c0..9b7d3 | Reload train_count, increment modulo16, store; return65535. Stack deallocation is one dead POP EDX. |

Every CFG decision, constant, callback publication and return is represented
in current source. There are no unseen rejoins or loop bodies. Two average
stores are retained because the short *mag argument may observe the first
store; the read comes between them. The original's integer machine wrapping
and signed shifts are the deciding semantics, not a new mathematical or
modern-compiler correctness claim.

## Bounded pair evaluation: pressure moves, no preimage

Three fullTUs: unchanged, two ordinary distance assignments with all deltas
still captured first, and second-pair evaluation deferred until after the
first sum/shift. Final short narrowing and both independent shifts stay fixed.
The original first pair sum/shift completes before the second pair completes;
source single combined expression creates live delta pressure. This motivated
an ordinary source-factoring test, not register/declaration permutations.

All three cells retain eight SI multiplies and seven arithmetic right shifts
through21observations: initialRTL,CSE,GCSE,combine,localallocation,globalreload,
late scheduling. InitialRTL131 instructions; afterCSE/GCSE111; combine/local
allocation101 for all three. The source controls change patterns and lifetime,
so the negative is not an inert/broken generator. Separate sums remains443B;
streamed pairs429B, neither413B exact.

At25.greg, baseline inserts SI store UID236 tosp+4 and sign-extends its lowHI
at UID51 into the named eq delta. This spill is absent before global reload.
Streaming eliminates that eq spill but stores the completed distance asSI at
sp+0 (UID235), consumed by final compare UID173. Current I remains a separate
stack home, moved fromsp+0 tosp+4. Both source families require an8-byte frame;
original keeps distance inECX and currentI alone on a4-byte frame. This is
spill-role migration, not recovery of the original source or frame. No arbitrary
slot, register or declaration-order search follows it.

## Independent index-read boundary: closed

The original signed n_out read9b698 follows lms_on clear9b685 and sym_count
store9b691. Source captures n_out atentry; retained machine loads it before
both writes. Four controls cross only a late existing index assignment with
the already measured streamed pairs. Baseline/late-index443B; streamed/both
429B. Both new cells change target code but produce no exact function.
Potential overlapping owner/index storage is a synthetic component-alias
question, not a proven modem history or a fabricated admissible fixture.
No semantic adoption follows the partial byte result.

The repeated baseline and streamed objects are raw-identical to their earlier
package controls. Seven compiled cells cover five distinct source forms;
35full function comparisons and49RTL observations. Epoch alone changes;
constructor/delete/equalizer-training/decision bodies stay canonical-identical.
Complete symbol/type/binding/visibility, allocated nontext/data/BSS and
nontext relocation inventories unchanged. Saved object/source hashes and
live canonical verdicts are rechecked. Zero exact gains/losses; no source or
header adoption. Known spill-control assertions fire on UID51 versusUID173.

## What this does and does not settle

This trace excludes a missing CFG/callback/loop in this body and closes these
finite factor/read families under the retained compiler profile. It does not
prove all original source expressions, owner declarations, alias contracts or
compiler options are exhausted. In particular, the original final counter
reload versus retained cached counter phi still needs a source/alias proof;
adding a fake spill, volatile or helper merely to retain the reload would not
supply one. Further experiments require an independent operand or source
witness. No helper call-boundary divergence was measured here, so calling this another
confirmed inline-budget symptom would exceed the evidence. Fully inlined
original helpers or TU-level interactions remain possible source hypotheses.

Reproduce with tools/fax_epoch_pair_factoring_reproduce.py and
fax_epoch_index_boundary_reproduce.py using the respective domain and retained
baseline directory; audit with python3 tools/fax_epoch_pair_factoring_trace.py.
All Gentoo3.4.2-r2 complete commands/selected assembler2.15.92.0.2 identity,
DSPLIB_REPRODUCE_BUGS-last flags, sources/headers/hashes and allRTL stages are
saved under build/fax-epoch-*. No runtime/fuzz/mutation or profile fitting.
