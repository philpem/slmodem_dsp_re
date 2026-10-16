# Uref stack-slot domain

Baselinef2cdb66a afterPR264, strict1045/1852. Other agent owns structure
inventory/cleanup. This pass changes no type layout/field naming and does not
enter issue22's trace queue. No fuzzing/mutation execution.

Reproduce two complete ADID TU cells: retained source and the previously
validated Uref double-half control. Record complete Gentoo3.4.2-r2 flags,
executed assembler, driver command and raw baseline. Observe unchanged
installed cc1plus stack-slot requests/returns and callers, preserving emitted
assembly and driver object raw. Require target events and completion checks.

Target: original updateUref240B allocates0x10 while the double-half control
allocates0x14. All79 decoded instructions otherwise agree, and ten residual
bytes derive from that four-byte shift. First identify actual slot mode,
size, alignment and allocation phase/caller, then inspect its RTL lifetime.
No hypothetical dead-local padding, function/declaration permutations or
stack-offset comparator exception. Unsupported observations refuse rather
than produce an empty clean report.

A bounded source cross is allowed only after the slot/lifetime witness
predicts it. Preserve stores, float reciprocal/mean, call boundary and saved
code age. Full-TU binding/data/canonical destination/bystander audits required
before source adoption; one final period/structural gate for an adopted batch.
Stop/reframe a raw-inert domain or unidentified slot instead of spelling sweeps.

Valid observation rules out an extra local: both retained and double-half
allocate HI2,HI2,QI1 and zero-size BLK alignment, ending at8 local bytes.
Final frame adds8 outgoing bytes and4 padding2 because the known callee
requires128-bit incoming alignment. Calls.c propagates cgraph_rtl_info.
The original unitePhasesInfoOfUref has a constant quiet-NaN load and no CALL;
retained initializes bestVar with nanf("") and contains a library CALL.
The direct-math rewrite (F11201) postdates F7848's builtin-constant controls.

New independently witnessed four-cell cross: Uref double-half and callee
nanf("")→idiomatic NAN macro. Prediction: remove only the callee library call,
recover a leaf's lower incoming alignment, then the caller's padding2 disappears;
only the crossed candidate should recover Uref240B. Original .rodata float
NaN payload must match the selected macro. Require baseline raw reproduction,
whole-TU imports/metadata/data/destination/bystander review and observed compiler
alignment before adoption. Do not initialize bestValue or repair D281. No
profile, definition-order or structure-layout change.

The same complete src/include screen finds four remaining nanf("") initializers,
all in ADID (two) and ConstellationDesigner (two). Original porcessSecondStudy
loads quiet-NaN at0x41cb0, no corresponding library call. A four-cell follow-up
crosses this second ADID initializer's NAN macro with the validated Uref
predecessor (first NAN + double-half), retaining the nearest/second-nearest
comparison and bestAt initialization. The second alone must recover the original
constant/no-call region; a whole function miss is not adopted for a score.
The designer pair is delegated in a separate bounded four-cell domain with no
production or structure edits. No additional NaN source spellings.
