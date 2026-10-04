# Voice status mapping control flow

902f47fa baseline, complete Gentoo production profile and raw TU control.
Four cells: retained if/return, switch/return, switch assignment into incoming
status, if assignment into incoming status. No new reads, diagnostics or data.
Blob _handle_status loads the incoming default into EAX at entry and branches to
three constant-return arms. Retained source materializes each constant before its
comparison and loads the incoming status only on the final default edge.
The recovered ConnectionEvaluator result-lifetime lever supplies a distinct
small source hypothesis: preserve an entry-owned result through dispatch.
Prediction: switch assignments or if assignments recover the entry default and
sparse dispatch; source branching is tested rather than forcing a register.
Falsifier: no strict gain or loss of an already-exact inlined caller closes this
family; inspect whole voice.c, not only the exported helper. No padding or
extra carrier local/type/flags permutation is part of this domain.
