# Generic SMC scalar and table capture groups

Pinned 856c1ecb full TU. Original SMC_encoder prologue 0x9fa28..a8 captures rot_step, rot_mod, pmask, qmask, amask, qshift before the table group pmap, imap, cosine, sine, qmap. Baseline captures tables first then scalar fields; both spill the groups across the loop. The original capture group is directly visible before any output/store/call, rather than inferred from a register target. Original local table pointers remain live across both complex output calculations.

Predeclare only baseline versus the observed scalar-before-table declaration group, keeping source order within the original observed scalar and table groups. Other ring/state declarations, selectors, count, loop, arithmetic, pointer advances, narrows and stores remain unchanged. No permutation matrix, width change, allocator constraints or source padding. All reads are ordinary captures before writes, so the change preserves values and alias-visible behavior. Full TU bystanders and complete metadata/named-data/canonical nontext audit required. A gain requires the targeted generic body itself, not a bystander alone.

## Measured outcome

Both generic encoders remain SIZE17 (609 reference bytes, 592 candidate bytes). All four bystander verdicts remain unchanged, including exact SMC_init. No targeted gain or loss; no adoption. The observed declaration groups do not establish a source preimage under this period profile.
