# Floating tone ring narrowing and MAC cursor

Four controls on both detector and filter, plus rawbaseline/owner-only seed.
Detector owner seed previous coefficient/state pointers. Cross (1) increment
short idx as statement before conditional clamp, against original conditional
with repeated casts, and (2) explicit backward delay pointer per MAC arm,
preserving signed int j and coefficients forward. Blob cwtl before shortcmp
then setl/neg/and; existing source cwtl aftercmp and branch. Blob MAC derefs
backward pointer ECX, including +len*4 between arms, while ours indexedbase.
No type/operation reordering, no out-of-range new domain, no new flags.
CompleteTU baseline, allocated data/symbols/nontext/relocs and allbystanders.
No harness/mutation/fuzz.

Measured result: 5cells bestdetectSIZE19, filtercursorSIZE33, no gain. CompleteTU proof included tools/gcc3_batch50_integer_audit.py124/124; no adoption.
