# V8 captured decision: declared before compilation

Base38626248; complete V8Dpsk.c only, retained Gentoo flags and reproduce-bugs
last. No production/header/central-document changes, profile controls or runtime.
PR263's V34 ownership remains excluded.

Original coefficient provenance: struct v8_v21 starts0xdd8 with a,b,c,d at
dd8,ddc,de0,de4. Loads78d42/48/5e/64 retain those pointers in stack1c/14/10/0c.
Both convolution loops accumulate a into stack0, b intoESI, c intoEDI and d
intoEBP. After narrowing and squaring, EDX=c*c+d*d=e_mark and
EBX=a*a+b*b=e_space. SUB78e49;TEST78e4b;SETG78e4d thus captures positivity
of wrapped mark-minus-space before silence evaluation. Dispatch78e6b..7d
compares1, branches unsigned-below for0, then compares2 for shared silence
clearing. Retained source has only nested conditions and no three-value owner.
Scoped prior controls cover fsktx and run helpers, not this decision family.

Exactly five cells: unchanged raw baseline; existing published-countdown
helper control; captured direct comparison with if-chain; captured wrapped
difference with if-chain; captured wrapped difference with switch0/1/2/default.
No direct-switch cell. Preserve retained run-arm bodies and helper boundaries.
Evaluate direction before silence, then assign decision2 when silent. Default
does nothing. Wrapped spelling uses unsigned subtraction converted to int
before positivity, matching original i386 arithmetic without signed subtraction
overflow UB. No arbitrary declaration, statement, register or slot permutation.

Difference is observable algebraically outside silence: energy INT_MIN from
two -32768 squares versus50000 from200*200+100*100 makes wrapped positivity
disagree with direct signed comparison. These are internal-value controls,
not claimed reachable modem histories. No runtime fixture is introduced.

Audit every emitted body, symbols/bindings, allocated data/BSS and canonical
nontext relocations; baseline must raw reproduce build/production-before and
exact losses must remain zero. Inspect initial RTL through final assembly for
captured subtraction and three-way dispatch. If case2 folds away, record the
first stage and stop rather than inventing a helper to force a switch. Complete
all five cells even if sizes improve; a near match is not adoption.

## Bounded results

All five cells compiled successfully and the unchanged full TU raw reproduced.
Actual symbol sizes (not signed interpretations of the absolute SIZE verdict):
baseline1121, published-countdown1061, direct-if1077, wrapped-if1093,
wrapped-switch1097, versus original1100. Twenty emitted-body comparisons and
twenty original-common verdicts preserve all three bystanders, symbol records,
allocated data/BSS and canonical nontext relocations; zero exact gains/losses.
The helper control's source and complete raw-object hashes match the earlier
run-owner package exactly, despite the different experiment directory.

Initial01.rtl for wrapped-switch owns decision pseudo122: subtraction of
e_mark/e_space, GT zero converted to integer, silence assignment2, and dispatch
comparisons. The09.loop stage retains the decision; final assembly retains
SUB/TEST/SETG and CMP1/CMP2. Case2 does not disappear. Twenty stage observations
cover each cell at01.rtl/09.loop/25.greg/35.mach. Named-pseudo annotations vanish
after allocation; their absence is not elimination evidence.

One independent source discriminator remains: original case dispatch uses JB
after CMP1, while the signed-int switch emits JLE. An unsigned decision carrier
is therefore a separately evidenced follow-up, requiring its own predeclared
control. This five-cell domain is closed without production adoption.

Artifacts: build/v8-decision-owner/results.json, full-tu-audit.json and
decision-stage-observations.json; each cell retains source, actual compiler
command, assembler identity and all RTL stages. Replay:

```
python3 tools/v8_decision_owner_reproduce.py --domain docs/v8-decision-owner-domain.md --baseline-dir build/production-before
```
