# V17 slicer tick ownership and absolute-value nodes

Pinned856c1ecb fullV17rxdec.c. Original 16pt tickMOVZWL,INC,CMPword,store hasno interveningMOVZWL narrowing; source localunsignedshort n=(ushort)(sym_count+1) emits redundantextension beforecompare. Sharedhelper becomes in-place memberincrement followedby equality clamp, retainingexact exitvalues/all callers and16bitwrap. Distinct16ptoriginalmetric usesbranchless32bitabs foreachchosenI/Q0x984ca..da, whilecurrent sums two ternaryabs inline and second lowersbranch. Transfer Playbook ABS_EXPR boundary: computeib=__builtin_abs(ib);qb=__builtin_abs(qb);n=(ib+qb)>>13. Tablesboundthesevaluesfarinsideint range. Four cells crosshelperupdate versusonly16pt absnodeboundaries. No other searchmetric/arithmetic/fieldtype/controlbranch changes, noorderingpermutations/flags/header modifications. Rawbaseline/full7body/data/metadata/relocs and everytickcaller audit mandatory; no scopepastthisfinitecross withoutnew independentwitness.

## Measured outcome

- baseline: all symbol verdicts unchanged.
- member-tick: {'FAX_FSE_decision_128pt': ['SIZE', 22], 'FSE_Bridge_det': ['BYTES', 94], 'FSE_decision_eqtrn': ['SIZE', 14]}.
- abs-nodes: {'FAX_FSE_decision_16pt': ['SIZE', 1]}.
- member-tick-abs-nodes: {'FAX_FSE_decision_128pt': ['SIZE', 22], 'FAX_FSE_decision_16pt': ['SIZE', 1], 'FSE_Bridge_det': ['BYTES', 94], 'FSE_decision_eqtrn': ['SIZE', 14]}.

No new exact symbol. No source adopted. Raw baseline and all complete compiler commands, period identities, source/hash inputs and RTL dumps are retained in `build/gcc3-batch100-fax-fse-tick-abs`. Complete TU audit includes metadata, named data, canonical nontext relocations and every function, including nonexact bystanders.
