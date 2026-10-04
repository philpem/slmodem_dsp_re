# Batch20 DSP boundaries declared before compilation

Baseline93d7eee1 full retained profile, mandatory bug define, actual Gentoo
compiler and selected assembler, unchanged headers. Raw complete-TU control
must reproduce production. No fuzz/mutation or per-cell phase execution.

FPM_iir_filt eight cells: baseline plus independent short sentinel countdown,
per-operation coefficient/state advancing cursors (blob increments state after
history shift, coeff after pair), and a distinct short section-output carrier
returned rather than returning initialized acc_in on zero sections. The blob
return uses a register not defined on zero sections, existing fixture excludes
that undefined path (D393). No invented initializer for that output. All source
paths preserve each valid section's arithmetic grouping and narrowing; block
caller inlining is reviewed as a bystander, not silently assumed unchanged.

FPM_lmsupd and FPM_lmsupd2 four cells: baseline plus histogram index
post-decrement at load boundary and advancing coefficient destination captured
at iteration entry, independently crossed. Blob index narrows after decrement
before coefficient product; pointer advance precedes product. Existing short
counter and arithmetic casts retained. This can change exact bodies and RTL
lifetimes without register-specific source or arbitrary permutations.

Closed prior domains: FPM_block_update position widths, phasor fraction/source
word widths, FPM_TONE loop widths/conditional-zero forms, Notch arithmetic tree,
FPM_SDM definition orders. None repeated here. Audit all functions, symbol
metadata, data objects/nontext and canonical relocations. No gain is adopted
with an unexplained bystander loss. Stop each axis on no gain and inspect an
independent mechanism before extension.

## Follow-up domains, declared after first controls

The first cross closes FPM_lmsupd150B exactly, but same transfer to lmsupd2
increases frame/spills and loads coefficient before first product, opposite
blob. Independent source use boundary: first-stage t computed before advancing
coefficient destination, crossed with t's short declaration (blob cwtl). Four
cells on the existing index/destination seed plus raw baseline and isolated
first-function winner. Predict delayed coefficient-value load and reduced
widx interference; if all miss close, no declaration permutations.

IIR first cross no hits; best countdown/cursors has1byte size gap but wrong
compare and real-product accumulation lifetimes. New independently observed
word-feedforward and decrement-test boundaries: post-decrement section loop
(`for(i=sections;i--;)`, matching DEC/inc/test CFG) crossed with short ff
(only used narrowed) and short acc_in (already narrowed after every section),
on the cursor seed; eight cells plus unchanged production baseline. Compare
whole body and inlined block caller; no fabricated register allocation knobs.

## FSM independent domain before compilation

FPM_FSM_modulate's blob has advancing bit cursor, two unsigned-short decrement
loops, unsigned-short running total, configuration scale and unsigned-short
sample length captured before calls. Retained source instead indexes bits,
counts up in 32bits, returns truncated int total and reloads configuration
inside the loops. Declare16-cell independent cross of those four boundaries:
countdown widths, bit cursor, cached config reads, narrow unsigned total.
Configuration length signed field is consumed unsigned in blob; negative
configured length is a component-boundary fidelity difference, not a reason
to retype the header. Record if recovered instead of claiming old behavior
unchanged. Loop bound negative-component tests need enough output allocation.
No helper inlining budget changes, flags or arbitrary statement permutations.

## Circular dot-product transfer declared before compilation

FPM_circ_dotp2 has the same short circular index as the proved LMS pair and
blob195B vs ours179B. Cross hist[index--] source load/post-decrement with
captured coefficient pointer then stride advance before multiply; four cells.
Coefficient and hist both const; existing shift range and arithmetic grouping
remain. No normalization-helper family repeated; unrelated FPM_div_32 remains
bystander. Stop on misses, no stride/type/header variants.

## MTD observed remaining boundaries declared before compilation

MTD_create previously recovered, unchanged here. MTD_detect blob advances
input pointer, short post-decrement counter, compares wideband short to a
memory word, clamps out-of-band with SAR15 (ours SAR31), and merges final
return1 edge rather than SETL. Declare16-cell cross of countdown, input
cursor, short wideband/tone/out_of_band carriers (all already truncated at
uses), and ordinary final return branch. Fixed grouping of filter calls,
products and smoothing retained. Negative count executes65535 samples in
blob, while retained ascending int skips; any exact recovery must record
this corrected input-boundary behavior, not claim purely inert rewrite.
No header, allocation or flags changes. Stop if none exact before any
more register/declaration spellings.

## FSM independent lifetime/return cross, authorized overlay before compile

Initial16cells miss; all-boundaries cell230B vs blob229B. New observed
boundaries: blob keeps a derived state->scaled pointer before calls, loads
signed sample length then widens only for consumption, and returns a
zero-extended count without the current signed-short return extension.
Root allocates only include/dsplib/fpm_fsm.h overlay for this return diagnostic.
Nine full TUs: raw unchanged baseline plus eight owner-pointer × signed-short
cached length × unsigned-short signature combinations on the fully crossed
seed. Header overlay accompanies return-width cells and actual overlay path
and digest recorded. No other header changes. All four source callers cast
FPM_FSM_modulate to unsigned short into unsigned nsamples; inspect reference
consumer extensions too before adoption, do not infer signature from high EAX
bits alone. Signed cached length remains assigned to unsigned short inner
counter: the negative-value full-word traversal is retained.

## Measured ledger and adoption

Initial12 valid complete TUs: IIR8 misses; LMS4 has one exact150B FPM_lmsupd
cell (index post-decrement and captured advancing destination). Neither axis
alone closes it. Applying same spelling to lmsupd2 misses with an extra frame
slot and earlier coefficient load. Follow-up15 TUs: IIR9 misses; LMS6 has two
combined exact cells150B+182B. Computing first narrowed product before capturing
the advancing destination closes lmsupd2; int/short t give raw-identical exact
objects, so keep its existing int declaration. The isolated lmsupd-only cell
still moves non-exact lmsupd2 through TU allocation state; adopting both winners
preserves the actual FPM_block_update bystander raw body instead.

In initial RTL, early dest capture has UID33 before t UID43 (second loop75<85);
late capture has t UID40 before dest UID44 (83<87). Both differ already at
expansion, then differ through combine before allocation; this is a recovered
source-use boundary rather than a register spelling. All products, rounding,
short intermediate cast and short circular comparisons remain as before.

FSM16 initial and9 follow-up cells: no exact gain. Complete-boundaries form
230B vs229B; authorized return-width overlay plus signed length and array owner
reaches229B but BYTES49, so size is not adoption. All four production callers
cast returned low word to unsigned short then pass a signed-short sample count
to MRF; the blob's consumer narrows AX too. Full-EAX observations alone do not
uniquely establish public return type. Header remains unchanged; entire domain
closed. Remaining discriminator: why inner decrement stays after retune calls
in blob but current source hoists it before the bit loop.

MTD16 cells: no exact gain. Countdown/cursor/narrow-state controls approach
size but not complete bytes; final ordinary branch canonicalizes with ternary.
Circular dot-product4 transfer cells: no exact gain. No adoption for these
or IIR negative domains. Two generator-invalid runs (whole-token coefficient
substitution, seed/original slicing) are archived as generator-invalid folders,
excluded from all reported valid denominators and rerun after correction.

Audit72/72 complete TUs: all symbol metadata/bindings, named data bytes and
relocations, allocated nontext and its relocation targets preserved; no exact
losses. 185 function/pass records and6/6 positive source-boundary controls.
Adoption only src/dsp/fpm_adeq.c: two functions become exact, +332bytes;
FPM_block_update remains raw unchanged. No fuzz/mutation or behavioral harness
execution here; batch owner runs deciding phase once after integration.
