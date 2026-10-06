# Receiver wrapper accumulation boundary

Twelve full-TU cells (3TUs × baseline/truncated-SI-accumulator/memory-count-
predicate/both), e0052eec. Original V17/V21/V27RX_modem stores signed truncated
accumulator in full-width stack home, executes ADD then CWDE each iteration,
and final looptest is HI TEST of caller count shared after callback. Retained
short accumulator can live in SI callee-save register but its source object
is HI; cached ushort remainder is zeroextended for arithmetic and uses SI
TEST. Candidate int total keeps existing explicit(short) assignment after
sum, so every wrapped value is identical; memory-count test reflects original
caller-owned HI predicate, preserving unsigned conversion for input advance.
No callback prototype, data layout, count/interface/call changes, no added
stores or declaration-order permutations. Full raw-baseline and all emitted
bodies/metadata/data/BSS/relocation audit. Miss closes this bounded family.
No runtime/fuzz/mutation and no arbitrary slot/register-cursor fitting.
