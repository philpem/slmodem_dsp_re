# Dead exported stability predicates: explicit common zero and operand direction

The original check_for_valid/easy share an early zero return and finish with
MOV1, while source ends in SETNE. Original short comparisons use register
minus memory, whereas retained source reverses operands. Four complete-TU
cells cross a shared zero-initialized result/goto done graph with reversed
commutative source comparison order; leave types/API/loads/window bounds
unchanged. Require both complete strict predicates (52/34B) and no bystander
losses; no near-hit assignment/padding/flag fitting. No fuzz/mutation.
