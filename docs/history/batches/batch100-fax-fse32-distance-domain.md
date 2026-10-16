# V17 32-point distance predicate

Pinned856c1ecb fullTU. Original linear distance compares promotedrq and signedtable value in32bits then branchesJS0x982e8..ed; baseline compares signed16bitvalues andJLE0x6aa..ad. OriginalJS tests signof a32bit subtraction, not signedrelationaloverflowcondition. Predeclare baseline versus ternary condition `(rq-DECv17_ANA_QMAP[k]) >= 0` with identicalarms andoutershortnarrow. Bothsigned16bit operands imply mathematicaldifference in[-65535,65535], so signtest equals existingrelationalpredicate withoutintoverflow. PreserveouterCONDEXP/shortnarrow toavoid inventingbranchlessABS pattern refutedbyoriginal; all searchbounds/distancearms/ties/metrics/tick unchanged. TwofullTU cells rawbaseline/all7bodies/tables/metadata/relocs. No axisexpansionwithout independentwitness.

## Measured outcome

- baseline: all symbol verdicts unchanged.
- distance-sign-predicate: {'FAX_FSE_decision_32pt': ['SIZE', 4]}.

No new exact symbol. No source adopted. Raw baseline and all complete compiler commands, period identities, source/hash inputs and RTL dumps are retained in `build/gcc3-batch100-fax-fse32-distance`. Complete TU audit includes metadata, named data, canonical nontext relocations and every function, including nonexact bystanders.
