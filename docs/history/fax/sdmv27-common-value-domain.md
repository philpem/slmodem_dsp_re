# SDMv27 initializer: common configuration value

Baseline90c7b22e. One original-use hypothesis plus raw baseline, full V27_SDM.c
TU, unchanged Gentoo flags and bug reproduction. Prior not-two fallthrough
control is closed and is not repeated.

New evidence from the zero-search target: no scratch allocation or renaming
changes its patterns. The local/global allocation dumps contain two separate
HI temporaries for the alternative config loads, allocated before the owner
pointers. The original has two alternative MOVZWL loads feeding one common
nbits store at0x9a83f. Test the idiomatic single assignment through a conditional
expression, keeping unsigned-short field type, all store order, and the later
unguarded cfg read (including the original null-config defect). No declaration
permutations, register constraints or alternate widths are in this domain.

Prediction: a common result pseudo changes the allocation problem, without
changing the observable instructions/operands apart from registers. Falsifier:
identical allocation or a changed CFG/memory-use shape. Compile raw baseline,
grade all3 bodies and review symbol/data/relocation metadata. A closer register
score alone does not establish recovery; adopt only a supported complete exact
initializer after the deciding gate. Otherwise record/close this one hypothesis.
