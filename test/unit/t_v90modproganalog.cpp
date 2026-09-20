/*
 * t_v90modproganalog.cpp -- `V90Modem::progress`'s analog-arm graph compare,
 * in its own binary on purpose.
 *
 * The parent fixture `t_v90modprog` drives the transmit side and the analog
 * arm.  On the modern tier the analog arm's `compare_graph("after the analog
 * arm")` fails six checks, all of them floats inside the embedded
 * `ResamplerTiming`: `lastHalfBaudErr` (+0x54), `bpfSq1` (+0x68), `bpfSq2`
 * (+0x6c), `bpfZ1` (+0x70), `bpfZ2` (+0x74) and `errZ1` (+0x78).
 * `ResamplerTiming::timingCorrection` computes
 * `e = 0.00025f * sq - 0.0005f * bpfSq1 + 0.00025f * bpfSq2` and feeds it
 * back as `lastHalfBaudErr`; GCC 14 reassociates the three-term sum.  The
 * object at 0x35806..0x35839 keeps the object's grouping -- the source
 * already spells it -- so no source form recovers the modern result, and the
 * feedback path compounds it over the block (157 to 4208 ULP).  F11369 wrote
 * this disassembly as the next test.
 *
 * WHY ONLY THE COMPARE MOVES.  The parent keeps the analog forward itself
 * (`MODEM(0)->progress(...)`) and its transcript, bit-block, count and
 * sample-block checks, so the `v90modprogmodem` suite's mutation of the
 * `demodulator->progress(...)` forward is still caught there -- only the
 * divergent float graph compare is lifted out.  This file re-includes the
 * parent with `ANALOG_ARM_AFTER_ONLY` defined; the parent's `run_analog_arm`
 * then skips every check except `compare_graph("after the analog arm")`.
 *
 * The task's rule (findings F2157, F3002) is that an excused binary exits
 * non-zero, so it cannot score a mutation set: this binary has NO mutation
 * suite, and `t_v90modprog`'s five transmit suites stay scoreable in the
 * parent.  `make period` passes this file with the object's own compiler and
 * flags.
 */

#define ANALOG_ARM_AFTER_ONLY
#include "t_v90modprog.cpp"
