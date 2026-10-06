# V32 silent transmit ring countdown and ownership

Basee0052eec, eight complete V32int TUs. Original TxNoCarrierV32 uses unsigned
short post-decrement motif, current ascending short induction. Original ring
wrap generates SETL/NEG/AND destination mask; current distinct next ternary
stays branched. Apply existing destination-preserving conditional-zero lever.
Original root-relative fp loads cache buffer/limit/state without retaining two
inner owners; compare established fp root fields against existing ring/smc
locals. No new type/layout, offsets or register declarations.

Cross all three visible boundaries, retain calls, writes, count widths and
wrap comparisons. Audit every emitted TU body/data/binding and exact set.
Nearest size is not accepted; no callback return signature is changed.
