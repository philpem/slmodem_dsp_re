# V34 receive-queue counter and cursor recovery

At baseline9565d51c, rxreadqueue is62B against the reference70B.
The reference has three independent source discriminators: a signedshort
counter (MOVSWL AX,BX followed by CMP BX,3), an advancing output pointer,
and a low-word sample read. The retained source instead has an int counter,
indexed output and a whole-word read followed by truncation.

The first finite domain crosses these three axes:
[declared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954231262).
Replay:

    python3 tools/playbook_v34_rxqueue.py --domain DOMAIN_URL

All eight complete-TU controls compile with the saved production command,
mandatory DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and its actually executed
assembler2.15.92.0.2. The unchanged control reproduces the raw production
object. Sizes in counter/output/read order are62,63,62,63,71,72,74,75B:
none is exact. The combined cell recovers the three observable operations,
but still introduces a next-cursor temporary through static q_next.

The next discriminating test addresses that helper boundary:
[three-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954276647).
Replay:

    python3 tools/playbook_v34_rxqueue_wrap.py --domain DOMAIN_URL

Production baseline, repeated combined cell, and combined with direct
conditional wrap are the complete domain. The repeated combined object
reproduces the first domain's combined object. Direct conditional wrap
updates the cursor with ADD4 and conditionally resets it to q->ring, matching
the reference's operation. It recovers all70bytes of rxreadqueue. This is
an ordinary ring loop with its source counter/read widths recovered; no
register names, declaration order or frame layout were selected.

There are11 valid compilation runs and9 distinct raw object emissions
across the two domains. The complete TU emits12 functions and5 named data
objects. Only rxreadqueue and its inlined caller V34agc change. Symbol type,
binding, visibility, imports/exports, named data values/offsets/relocation
targets and allocated nontext bytes are unchanged in every control.
No source from the excluded directory was consulted.

The existing fixed t_v34rx queue section calls each implementation40times
from each of64 ring start positions:2560 component calls per side. It compares
the entire buffer except actual pointer fields, which are compared as offsets.
These are planted component ring states, not a claimed public handshake
history. The same binary also tests V34agc; the full deciding gate covers
constructed V34 receiver and handshake paths. No fuzzing or mutation execution
is used. Static anchors are checked separately.

Production adoption, whole-tree census, partial-link measurements and gate
results are recorded in F11635 after validation. Retain the first domain's
negative cells; they show that matching source widths and traversal alone
does not eliminate the helper lifetime boundary.

Validated adoption changes exactly src_pump_v34_V34RX.c.o among300 period
objects, and its raw bytes match direct-wrap/candidate.o. Whole-tree exactness
is917→918/1852, with94,638→94,708 exact bytes; sole gain rxreadqueue, no losses.
V34agc is746B before/after versus827B reference. Its complete suffix from+0x60
and canonical relocation map are unchanged; differences are restricted to
the intended inlined queue prefix, whose operation is independently present
in the reference V34agc. The other10 TU bodies are unchanged.

Fixed Gentoo make phase passes386tests/0failures, with all structural gates
passing. Existing queue and AGC sections pass2,606,080 and3,410,880checks
respectively; these counts are checks, not independent generated inputs.
Static anchorcheck reports285suites/10038anchors,0detached/nonunique;
no anchor retarget is needed and no mutation execution is performed.

Complete300-object links in identical recovered order remain DIFFERENT
(strict exit1 before/after). Positioned equal bytes68,839→68,546/943,398
(-293); allocated bytes914,798→914,814 (+16). Exact section records70/92
and symbols394/2907 stay constant, while relocation records1026→1025/18317.
This measured function/source gain is not a positioned layout improvement,
whole-object identity or a unique original compiler-profile claim.
Artifacts are in build/playbook-v34-rxqueue{,-wrap,-adoption}; raw baseline,
all controls, complete-object audits, census and partial comparisons remain
available. No modern portability claim is made.
