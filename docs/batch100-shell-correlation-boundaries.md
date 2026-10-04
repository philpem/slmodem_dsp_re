# V34 shell correlation and word totals

Reference shellDemapper contains three inlined correlations with forward and
reverse advancing pointers, word countdown tested positive. Retained helper
has indexed lo/d operands; loop pass need not recover original independent
cursors. Reference group totals c,d and n compare as unsigned words at clamp,
while retained static helper uses int carriers with explicit assignment casts.
Eight-cell domain: independent t-forward/t+d-backward correlation cursors;
unsigned-short c,d carriers; unsigned-short static group n formal. All caller
n and assigned group totals already bounded to0..65535, so changing carriers
preserves values; pointer reads identical, including first and last ordering.
No external API/header edits, arithmetic/conditions/outputs unchanged. Current
helper reused only shellDemapper; complete TU bystanders/data/relocs audited.
No arbitrary helper inline-budget or register spelling controls.

Eight valid full TUs; no exact gain/loss. Reference489B, baseline495B;
unsigned-word n formal alone493B; pointer cursor only511B; combined
pointer+formal495B. Word total carrier is raw-inert on both axes. Only
shellDemapper changes, all12 symbols/16 named objects and nontext/relocs
audited unchanged. Close helper cursor/group width family; source remains
retained. No runtime, fuzzing, mutation or partial phase claimed.
