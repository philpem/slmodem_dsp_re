# V8 run publication and countdown: declared before compilation

Base38626248, unchanged Gentoo .build-config and reproduce-bugs last.
PR263 observed97e8a10c: exclude its V34 files/headers/Makefile/mutation snapshots.
Only complete src/v8/V8Dpsk.c controls; no production/header edits yet.

Original remainder stores precede bit pushing in all four run conversions:
mark/space flush at78eb8/78f0a, mark/space drain at7905d/78fcb. Drain predicates
reload stored remainder from memory before unsigned comparison. Original count
loops guard zero and backedge DEC/JNE. The prior unsigned-run × postdecrement
zero-only domain was valid but emitted DEC/CMP(-1)/JNE and left17bytes residual.
Publication occurs after the loop in the retained value-return helpers; drain
remainder publication can be eliminated because the caller then clears it.

Six cells: retained rawbaseline; unsigned run consumption/positive-loop prior
control; unsigned/zero-only-postdecrement prior control; unsigned published-run
helpers/zero-only-postdecrement; unsigned old helpers/guarded-do countdown;
unsigned published-run helpers/guarded-do countdown. Both helpers accept the
actual int run-field address, use unsigned read arithmetic, store *run&3 before
pushing, and return void. No alias casts, type changes or fabricated owners.
Guarded-do executes only for n!=0 and decrements after each bit. Rounded unsigned
counts are bounded0..2^30, so retained int push argument stays nonnegative.
Signed field increment wrap remains an unchanged independent typing issue.

Predict source publication restores store placement and memory-based rounding;
guarded-do restores DEC/JNE without CMP(-1). Inspect initial RTL then loop,
GCSE/flow and final output to identify elimination/motion. Falsifiers: source
publication disappears or moves after bit updates; loop still tests old count
or signed positivity; unexplained export/data/bystander changes. Complete all
six cells and stop/reframe. A residual size reduction is not adoption evidence.
No register/slot/statement permutations, profile fitting, mutation or fuzzing.
