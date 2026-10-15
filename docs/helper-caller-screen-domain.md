# Complete-source helper/caller expansion screen

Source baseline 7edfb734, production 1079/1852 exact, fixed retained Gentoo objects.
Replay may use a later documentation commit only if src/, include/ and period.mk
still match that baseline; the actual invoking revision is recorded.
Earlier repeated-call widening covered C++/DSP95TUs and data/service99TUs,
with original size caps4096/800 and V34 excluded. Fixed-index screen covered
283TUs outside V34, <=4096B; ten nominations were classified/tested and closed.
Do not retry those from identical counts.

Broaden to all299 retained C/C++ source TUs, all original sizes and V34;
the300th object is core/pow.S, excluded explicitly from the lexical source scan.
Reuse canonical call target counting and lexical numeric2..32 for-header/
conditional-backedge detector. Record source/object/config/census hashes,
exact exclusions, missing/unmapped bodies and indirect/interior calls. Nominate
repeated calls or fewer original backedges in fixed-loop bodies, retaining
header/signature ambiguity. Record direct in-TU caller edges where present.
Neither aggregate count establishes expansion or source repetition.

Known closed V92Precoder fixed6/12 positive must fire. Call-deficit detector
must fire on historical ConstellationPower5mod/6div versus1/2 rolled counts,
explicitly a recorded-count control rather than an archived object replay.
Current exact ConstellationPower supplies a live equal-call negative control.
Prior source/profile domains remain closed. Review novel nominees by full
disassembly/CFG and source before declaring bounded controls. No flags changes,
source edits, fuzzing/mutation or compiler interventions in this screen.

Scope limits: source .c/.cpp files only, not included headers; literal numeric
for bounds2..32 only, not macros or variable bounds. Symbol inventories count
emitted copies, whereas the production census counts distinct original symbols.
