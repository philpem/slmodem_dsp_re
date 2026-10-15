# V32 cleaned getter alias diagnostic

Pin f420cd89. Original53B reloads modem+0x68 after each output-count store;
retained47B retains that owner register. Source already reads FP(modem) at
return, so another repeated source expression is not a new discriminator.

Two unchanged-source complete-TU compiles: baseline complete retained Gentoo
configuration plus mandatory bug define, and diagnostic-fno-strict-aliasing.
The baseline must reproduce production raw. Record complete commands and inspect
initial/CSE/global/machine streams to locate the missing repeated owner read.
Prediction: withdrawing type-based alias disjointness retains the owner reload;
falsifier: its graph is unchanged or a different pass removes it independently.
Audit all six body verdicts and metadata/data. This is an explanatory option
control, not permission to change production flags or add casts/volatile/union
views merely to force reload. A new source choice needs original type/API evidence.
No candidate-runtime, fuzzing or mutation run; production source unchanged.
