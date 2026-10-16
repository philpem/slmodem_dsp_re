# ADID reset use-site counter width

Pinned856c1ecb. Original reset indexes its phase with MOVSWL at0x40370 but zero-extends the updated phase at0x40410 and uses unsigned JBE at0x40422. Inner code loop zero-extends updated AX at0x403db and uses JBE0x403e2; baseline sign-extends and uses JLE. Both loop domains are fixed small nonnegative integers (six phases and128codes). Prior verdict/common-result domains and reset field/order controls are different; no allocation/register fitting is proposed.

Predeclare four source cells: baseline; inner local ci unsigned-short; outer comparison receives unsigned-short phase while preserving signed index local; both. All bounds, narrow increment, initializers, stores, codec calls, parameter tests and diagnostic source remain fixed. Compile initial -dr as established ADID -da ICE workaround. Baseline must raw reproduce original period profile. Require target exactness, complete34-body metadata/data/reloc audit; changes to other nonexact bodies recorded, no bystander-only adoption.

Measured: Four valid -dr cells raw reproduce baseline, all retain reset SIZE54, no gains/losses. These source width uses do not reproduce the original loop body; domain closed.
