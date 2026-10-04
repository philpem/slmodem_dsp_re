# V92Jd phase return expression mode

Previous two-cell named float result boundary emits raw unchanged56B; ordinary SF local alone does not establish the original80B FSTPS/FLDS. New discriminant: source return conversion from a double expression versus an SF expression. The scale65536 is exactly represented and powers-of-two scaling of16bit unsigned phase is exact. Original memory scale isSF but that does not establish expression mode; earlier literal-width levers similarly retainedSF constant pools. Predeclare baseline versus65536.0 unsuffixeddouble literal solely in getJdPhase return. No result type change, volatile/artificialspill, cast matrix, loop/body order or flags. All other setters/ctor conversion literals unchanged. FullTU raw control/all bodies/data/metadata/relocs required; close ifinert.

## Measured closure

BothrawfullTU cellsvalid. Unsuffixedscaleisrawinert56B/SIZE24; all21bodies11exact anddata/metadata/nontext canonicalrelocs unchanged. No further literal/type/storage expansion.
