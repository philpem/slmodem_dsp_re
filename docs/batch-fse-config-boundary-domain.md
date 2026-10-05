# FSE mode/configuration boundary: declared before compilation

Base240481e6, complete fpm_fse.c with retained Gentoo profile and bug define
last. Original first four mode stores at40/44/48/4c precede configuration REP,
freq3c and phase70 follow it. Current generated code hoists both freq and phase
before REP. Source currently copies cfg before all initializations. Test only
baseline versus copy after four modes, before remaining scalar initialization.
This is one observed publication boundary, not arbitrary store permutation.
Cfg references point to a valid configuration; writes outside embedded cfg
preserve that ordinary contract. No synthetic overlap/reachability claim.
No helper/register/slot/volatile/flag changes or V34/header/shared-gate edits.
Full raw baseline/all emitted bodies/binding/data/relocation audit; stop after
two cells if motion still occurs. No runtime/fuzzing/mutation execution here;
only complete exact candidates can be retained after final combined phase.

Result: both complete raw objects equal (SHA2566a3fde3e9fe67b2dcd9a3cd03f6d75ebd11109d9af0b5d004152b2380dcc4e64). Four functions per cell; no body/data/metadata changes, no gains/losses. The observed stores are moved despite this lexical copy boundary. Family closed without adoption.
