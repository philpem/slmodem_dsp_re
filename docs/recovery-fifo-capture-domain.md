# Class1 FIFO result-capture domain

Baseline4169cb3b, cDATAtx.c complete TU; Gentoo retained source profile and
mandatory reproduction define. Independent original call/consumer witnesses:
_tx_nulls_state FIFO_write9d0d2->MOVZWL AX,ECX9d0d7; FIFO_read9d0fa
->MOVZWL AX,EAX9d0ff->store9d102. _tx_scrambled_ones_state FIFO_read
9d404->MOVZWL AX,EAX9d409->store9d40c. Current callers consume fullEAX.
FIFO_read/FIFO_write bodies compute unsignedshort counts and return those
values, so local unsignedshort captures preserve every valid return value
without changing the public return contract. FIFO8 is a separate closed domain.

Declare four cells: two read sites narrowed together or retained; null-state
write result narrowed or retained. Predictions: restore the three witnessed
MOVZWL consumer boundaries. Falsifiers: desired boundaries absent, unexplained
read/call changes, exact losses or invalid unchanged raw baseline. Do not
change FIFO APIs, _tx_data_state signed capture or any other source axis.
Retain full-TU data/metadata/body/export evidence; run existing component
fixtures labelled with their synthetic planted-state boundary and fullphase.
No new public-call reachability claim. Stop this domain after the four cells.

## Second domain: residual value owners and tail width

Opening four cells restore all three low-word captures, with no exact losses.
The scrambled-ones body is then BYTES2: only CMP operands/JLE versus original
JGE differ. It compares an invented rd local after publishing *tx_data_count;
test the published value as the comparison owner. Null-state remains BYTES20:
original loads the source pointer before writing state, while current call
expression loads it afterward; test one local acquisition at that observed
boundary. Its final capacity read is MOVSWL, original9d164 is MOVZWL; test
an unsignedshort cast at that read, without changing shared fifo field type.

Declare eight crossed cells over these three binary axes with the witnessed
FIFO captures fixed; add one unchanged-TU raw control (nine compilations).
No header/flag/register/source-order permutations. Each axis predicts its
exact own operand/width/acquisition boundary; no score-only interpretation.

Opening owners generator refused a duplicate match because the tail statement
also appears in prose. No compile cell ran; the invalid log is retained at
build/recovery-baseline/fifo-owners.log. Revised generator scopes the function
body and writes a separate recovery-fifo-owners-v2 artifact directory.
