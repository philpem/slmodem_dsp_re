# SGD quotient width and count condition boundaries

902f47fa retained profile. SGD_correlate blob narrows/sign-extends division
result before storing scale; ours scale is int. One short scale control.
Since numerator is1, every defined nonzero divisor result is -1,0,1; narrowing
cannot alter defined arithmetic. Both preserve the same zero-divisor fault.
Independently replace sentinel short countdown with short postdecrement test,
matching blob increment/test condition. Four correlate cells. Transfer only
that count condition to SGD_pattern_det (one separate cell), all body loops,
status writes and return index fixed. Full TU proof and explicit stopping.
