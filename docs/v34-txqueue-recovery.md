# V34 transmit-queue sample order and direct wrap

Atfcf0427a, txwritequeue is70B against the reference68B. The reference loads
the source sample before clearing the ring destination's highhalf, then stores
the lowhalf. Retained source clears the highhalf before its source read.
The reference also increments the ring cursor directly and conditionally wraps
it; retained static q_next introduces a next-cursor temporary.

[Declared four-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954478094)
crosses retained/low-first store order with helper/direct conditional wrap.

    python3 tools/playbook_v34_txqueue.py --domain DOMAIN_URL

All four complete-TU controls compile with saved production flags and mandatory
DSPLIB_REPRODUCE_BUGS under Gentoo GCC3.4.2-r2 and actually executed assembler
2.15.92.0.2. Production reproduces its raw object. Direct wrap alone is68B
but BYTES10; low-first alone remains70B/SIZE2; both recover68B EXACT. Four
valid compiles give four distinct raw emissions. No register, declaration-order,
frame or counter spellings are selected.

The TU emits seven functions and zero named data objects. Only txwritequeue
changes; all six bystanders and every symbol type/binding/visibility,
import/export and allocated nontext byte agree. Retain q_next for its other
actual caller rather than expanding this loop recovery into unrelated users.

The existing fixed t_v34rx transmitter section supplies23starts×60calls,
1380 paired component calls with separate source storage. It cannot detect
source/destination overlap. A new explicitly synthetic component probe starts
the source at the current interior ring destination's highhalf. Two cases use
old highhalves0x1234 and0xfedc. The blob captures that value before clearing it;
old reconstruction loses it. These are valid allocated component buffers,
not a claim that public modem lifecycle paths alias them. A fixed negative
baseline run must demonstrate the detector firing before adoption.

Validation and whole-tree/partial-link evidence are recorded in F11638 after
the fixed deciding gate. No fuzzing or mutation execution is used.

The known-baseline negative control fails6/2040overlap checks; both independent
blob witnesses preserve their respective old highhalves, while reconstructed
first samples become zero. Deciding period run reports0passed/1failed for
this deliberately failing single binary. After adoption, all2040checks pass.
The original separate-storage section still passes1,404,840checks.

All300production objects are inspected: only src_pump_v34_V34TX.c.o changes,
and raw bytes match the combined candidate despite the explanatory comment.
Whole-tree918→919/1852,94,708→94,776exact bytes; sole gain txwritequeue,
no losses. Fixed Gentoo makephase passes386tests/0failures and all structural
gates. Static anchorcheck remains285suites/10038anchors,0detached/nonunique;
no retarget or mutation execution is needed.

Complete300-object partial links in identical recovered order remain
DIFFERENT (strict exit1). Positioned matching bytes68,546→68,545/943,398
(-1); allocated914,814bytes and exact section70/92, symbol394/2907 and
relocation1025/18317records remain constant. Alignment absorbs the shorter
standalone function. Do not infer complete object/profile identity or a layout
gain. Artifacts are in build/playbook-v34-txqueue{,-adoption}, including the
negative baseline detector log, promoted object/census, full gate and both
partial comparisons. No modern portability claim is made.
