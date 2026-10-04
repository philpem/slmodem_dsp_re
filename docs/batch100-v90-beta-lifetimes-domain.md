# Equalizer beta diagnostic conversion and shift operand lifetimes

Pinned856c1ecb fullTU. Original beta setters use nonpopping FISTL of scaled at0x36628, preserve scaled for absolute whole andfraction. Current first conversion duplicates scaled anduses FISTPL. Predeclare explicit int whole=(int)scaled immediatelyafter scaled declaration, consumed in unchangedfractionexpression. Original shift count instruction precedes member shiftpublication0x3670c/0x3670e, whilecurrent stores shift before maskedoperand shift; explicit shifted local acquiredbefore shiftmemberpublication is distinct lifetime boundary. Retain existing defined mask helper and all casts/types/constants/arithmetic, no rawundefinedshift or padding. Four crossed cells baseline; diagnosticwholelocal; shiftedoperandlocal; both, applied consistently to setLinearEquBeta andsetDfeBeta. Otherhelpercallers unchanged. Existing F260mask retained; F11353 meaningfulcasts retained. No arbitrary variable permutations, flagfitting or narrowing. Rawbaseline/fullTU allbodies/nontext/metadata/relocs andpositiveexactbystanders mandatory; parentendbatchgate.

## Measured outcome

- baseline: all symbol verdicts unchanged.
- whole-local: all symbol verdicts unchanged.
- shifted-before-publish: all symbol verdicts unchanged.
- both-lifetimes: all symbol verdicts unchanged.

No new exact symbol. No source adopted. Raw baseline and all complete compiler commands, period identities, source/hash inputs and RTL dumps are retained in `build/gcc3-batch100-v90-beta-lifetimes`. Complete TU audit includes metadata, named data, canonical nontext relocations and every function, including nonexact bystanders.
