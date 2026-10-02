# V34 echo history-energy traversal control

A bounded eight-function screen at e70c56b9 covers three complete TUs with
90 functions; it does not classify the remaining tree. Its strongest fresh
source discriminator is V34EchoEstimateDelayLineEnergy: blob53B advances a
history pointer and decrements a captured count, while retained54B indexes
history through unsigned k. The authentic baseline loop dump rejects history
strength reduction (-108 versus8 benefit) and cannot eliminate the index;
there is no reversal in this function.

The [declared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5956020613)
keeps production and tests captured const short history pointer / unsigned
remaining count with a positive-count guard and do/while decrement. Signed
sample load, square, arithmetic shift5, wrapped unsigned accumulator addition
and final int return remain. Initialized owner with count0 returns0 without
history dereference; no null-owner contract is asserted. Positive count needs
a valid history array of that extent; final pointer is one-past.

Raw production baseline reproduces. Candidate38B/SIZE15 against53B reference
recovers direct sample load/ADD2 and DEC/JNE but merges the entry condition
with the final backedge, using TEST/JMP to that condition. Reference instead
has CMP0/JBE skip and a separate copied count before the body. Alpha rows17
versus18 also fail; this is not a pure register mismatch. No exact gains or
losses (11/26 unchanged). Only energy changes across26 functions and48 named
data objects. All other complete bodies/canonical relocations, types/bindings/
visibility/imports/exports, data values/offsets/targets and allocated nontext
bytes agree.

Two valid compiles/two raw emissions use actual saved flags with
DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and executed assembler2.15.92.0.2.

    python3 tools/playbook_v34_echo_energy.py --domain DOMAIN_URL

Artifacts: `build/playbook-v34-echo-energy`, including RTL and full TU audit.
Existing fixed t_v34ec setup initializes a32-tap echo bank, then performs2000
sequential update/filter/adapt/energy paired calls with valid history arrays.
This is initialized component history, not public modem lifecycle evidence.
The entire filter/adapt section's denominator includes more than energy and
must not be quoted as2000 isolated scalar checks. No new fixture execution,
candidate differential claim or source adoption.

Close this finite cursor/count control without pointer-end/guard/width/
declaration/register variants. Sibling EchoFilter/Adapt results neither exhaust
nor recover this distinct reduction; its own negative complete-body evidence
decides the tested family. F11655 records the control.
