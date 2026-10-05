# V8 tap signedness and sample cursor: declared before compilation

Base10ae345a. Full V8Dpsk.c, retained Gentoo profile, reproduce-bugs last.
No production/header edits, #22 writes, runtime, fuzzing or mutation execution.
PR263 scope observed97e8a10c; V34 files and shared gates remain excluded.

Boundary review finds only one original0xc2c pointer formation, in the target;
modulation forms0xc20 independently. Initializer writes full-object displacements,
not a typed receive pointer. Do not fabricate a subobject from this alone.

Independent operand witnesses: both convolution tap loops end JBE at78dbe and
78e1b, whereas signed j emits JLE. Original top selection and outer limit compare
are signed, so retain signed top/pos/limit and change only j to unsigned int.
Valid tap paths use pos0..11 and j0..39; modulo32 address arithmetic still selects
the same inbuf/delay elements. This does not claim validity for planted states.
Input at78cba is read through a cursor incremented by2, with counter3 decremented
and JNS. Source streaming four samples in increasing address order with a signed
countdown matches this witness; do not reverse sample order. Preserve inbuf_pos
update before store. This is not a physical register/slot/declaration permutation.

Five cells: raw baseline; repeated wrapped unsigned-decision/zero-bit control;
that control with unsigned j; that control with input cursor/countdown; both.
Complete the cross even if an intermediate size improves. Require raw baseline
repeat and prior control source/object repeat, full emitted-body/metadata/data/
BSS/nontext-relocation audit. Inspect initial RTL and final signed/unsigned
branches and input pointer/countdown. Zero or near hits do not justify adoption.
Stop this declared family after five cells unless an independent new witness
supports a different hypothesis. Source adoption requires complete exactness and
one final combined period gate.

## Results and next discriminator

All five full-TU cells compile; baseline raw-repeat passes. Repeated decision
control raw-object hash agrees with PR271. Target sizes: original1100B, baseline
1121, decision control1089, unsigned-index1041, input-cursor1089, crossed1057.
No complete exact gains/losses. Twenty emitted-body and twenty original-common
comparisons preserve bystanders, all symbol records/binding, allocated data/BSS
and canonical nontext relocations. Production/header files remain unchanged.

Initial RTL's two tap exits are GT/GT for signed j and LTU/GTU for unsigned j.
Final signed branches are JLE/JLE; unsigned JAEs first because the comparison
operands commute, then JBE. Both represent the unsigned <= tap predicates. The
cursor restores first input JNS while keeping ascending sample addresses. Four
witness controls include the opposite signed/indexed refusals.

An explicit LEA/ADD formation screen covers190779 decoded original instructions
and finds two matching formations: demod0xc2c and mod0xc20. This is bounded
instruction-form evidence, not a complete pointer/value-flow proof. No second
receive formation or typed extent is established; initializer uses full-object
word stores. The proposed receive-structure split remains unproven.

This domain is closed without source adoption. The next observable convolution
difference is operand capture: original first tap loads a[j] before capturing
the sample, whereas the retained source names a sample temporary before the
four products. Determine the earliest compiler stage at which these capture
orders differ before declaring another source domain; do not permute the four
accumulators or force registers from a size result.

Replay at10ae345a with its archived production objects/config:

```sh
python3 tools/v8_index_cursor_reproduce.py --domain docs/v8-index-cursor-domain.md --baseline-dir build/production-before
python3 tools/v8_index_cursor_audit.py
```

Artifacts build/v8-index-cursor/results.json and audit.json contain hashes,
commands, full-TU results, original pointer formations and initial/final branch
observations. No runtime/fuzz/mutation gate is run for this tools/findings-only
pass. No new differential or reachability claim.
