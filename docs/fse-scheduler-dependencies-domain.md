# FSE copy scheduler dependence diagnostic

Base6b4509bd, full fpm_fse.c, Gentoo retained profile and bug define last.
F11716 already excludes reset-store ordering and predicts the copy dependency
boundary. F11832 subsequently excludes the four crossed ordinary copy-form/
placement cells and measures sched2 as the first pass moving freq/phase_acc
above the copy. These source domains remain closed: this is a diagnostic replay,
not another adoption search or a new scoring domain.

Exactly two cells add only scheduler verbosity5 to retained source and the
previously measured delayed builtin-memcpy control. Require raw object repeats
of both earlier cells, unchanged complete compiler config except dump flags,
all emitted bodies/metadata/data/BSS/nontext relocations. Record dependencies,
priorities and ready/issue order around the copy and its two SI stores. Predict
different copy-related dependency edges if the source memory form explains
the contrasting placement; if priorities or unrelated hard-register effects
explain it, report that rather than inventing an alias cause.

No source/header/profile adoption, pointer/volatile/alias coercion, scratch
variables, statement/type/register permutations, runtime/fuzz/mutation work.
Identify what can discriminate original source after comparing the complete
dependency graph. Official GNU scheduler sources are explanatory evidence,
not a claim that Gentoo's patch stack has been completely recovered. Issue22
and PR263 remain read-only/outside edit scope.
