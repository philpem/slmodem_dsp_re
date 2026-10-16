# Faithful diagnostic scratch extent

Baseline 4169cb3b, complete encode.c TU, retained Gentoo profile with final
DSPLIB_REPRODUCE_BUGS. Domain: two cells, unchanged and conditional faithful
270-byte cEncodedTemp versus current unconditional271. D39 and original
local symbol establish the extent; current boundary fixture exercises131
characters and confirms the original writes its terminator into alignment
padding. Prediction: only the local symbol size and any consequent layout
change, with both canonical function bodies unchanged. Falsifier: changed
function operations, output differences, or unrelated symbol/data changes.
Do not compensate elsewhere for any size/layout change. Keep fixed mode271.

Validate the unchanged complete TU raw before reading results. Record all
commands and symbols; run existing t_encode boundary fixture under Gentoo
and full phase before retention. No new source spelling matrix follows.
