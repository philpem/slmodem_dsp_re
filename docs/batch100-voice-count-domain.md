# Caller word versus callee narrowing in detector_progress

Original voice_modem338B differs by only the MOVZWL caller count load from
source MOVSWL. The original detector_progress independently MOVSWL-loads
its incoming count word, so unsigned-short formal everywhere would erase a
real signed callee boundary. An int formal with a signed-short local can
express both observations without changing the ILP32 slot or low-word use.

Predeclared five consistent caller/callee/header cells: baseline; int formal
with original explicit-short caller; unchanged formal with plain caller;
int formal/plain caller; unsigned-int formal/plain caller. Preserve signed
short inside the callee in both wide controls. Compile both complete TUs,
review all shared body metadata/data and consumers, and require a strict
caller hit rather than normalize argument loads. No runtime/domain extension
or header adoption without matching low-word interpretation. Existing DLE
win is separate and must reproduce in final combined TU. No fuzzing/mutation.

Initial invalid run used include/include/dsplib overlay path: wide-int was rejected against the unchanged header. Preserve as invalid-overlay-prefix and exclude; correct overlays to dsplib/detector.h and rerun complete controls.

Corrected10/10 complete-TU cells reproduce both raw baselines. Neither formal width alone nor caller cast removal alone changes the bodies; combined wide/plain forms change only voice_modem from BYTES2 to BYTES3 and miss. No gain/loss/source/header adoption; unsigned-short formal rejected by original signed callee load. This bounded width family is closed.
