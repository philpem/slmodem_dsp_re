# Used quality verdict and independent fax operand boundaries

Baseline `8af3af53`, following merged PR #267; unchanged Gentoo period profile,
all configurable flags from `build/production-before/.build-config`, bug define
appended by the shared experiment helpers. No fuzzing or mutation execution.

Original `RxHdxDataV17/V27/V29` produces a quality boolean using SETNE, then
NEG/AND selects the demodulated count. A direct mask or multiplication spelling
is already folded into branches in initial RTL. The independently captured,
used `int reliable` verdict retains a value that permits the original mask.
The result remains unsigned-wide until the terminal short return.

Finite controls:

* `next20_count_mask.py`: baseline plus mask/multiply, short/wide result,
  five cells per TU; all fifteen miss.
* `next20_count_predicate.py`: baseline plus mask/multiply crossed with count
  selection before/after the original low-SNR flag clear, five cells per TU.
  Both mask cells reproduce V17; neither multiplication cell does. V27 remains
  three bytes long; V29 remains eight bytes long.
* `next20_count_cross.py`: captured mask crossed with two independent original
  operand witnesses. V27 caller zero-extends the count, while its callee
  consumes a signed short (F9237); use unsigned-short public count and explicit
  signed-short consumption. V29 original writes the existing flags byte,
  whereas retained source writes its word. Four cells per TU, including every
  direct V27 header consumer. Each combined cell reproduces its target; the
  independent cells do not. No structure definitions or layouts change.

`next20_count_audit.py` checks all emitted function bodies, complete symbol
metadata/bindings, allocated data, BSS and canonical nontext relocations.
V27 callee and all non-primary consumers are raw byte-identical in every cell.
Its protocol caller also changes at the same declared API boundary and remains
non-exact; it is included explicitly, not hidden as a bystander.

Measured domains: 102 valid complete-TU cells, 704 strict common-body verdicts;
three distinct gains, 226 reference bytes each. No exact losses. Raw unchanged
baselines reproduced. Integrated production reproduces the three winners raw; final census is
1,055/1,852 exact and the period gate passes 388/0.

This reopens the earlier negative formal/flag-width controls through a new
independent optimizer boundary; it does not repeat their single-axis domain or
claim a unique source preimage. Source-visible boolean ownership can matter
before register allocation even when algebraically equivalent direct expressions
have already folded to control flow.
