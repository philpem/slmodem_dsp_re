#!/usr/bin/env python3
"""test_preempshape.py -- regression test for the V.34 pre-emphasis selector.

    python3 test_preempshape.py          # exits non-zero on any failure

WHAT IT PINS, and why each case is here rather than being a nice idea.

1. REACHABILITY.  Every one of the eleven templates must be selectable.  The
   object's counter can only ever return 6..10 -- its counter starts at 5 and
   is advanced before its test, so indices 0-5 are unreachable and the author's
   own `return 0` arm is dead code (deviation D53).  That defect is the whole
   reason the shape matcher exists, so a test that does not check all eleven
   would not notice it coming back.

2. IDENTITY.  Given a channel that is the exact inverse of template i, the
   selector must answer i.  This is the strongest statement of "it picks the
   optimum" that can be made without a channel model, and it fails loudly if
   the template encoding, the band limits or the bin-to-frequency mapping drift
   apart.

3. BROADBAND NOISE.  Per-bin Gaussian error.  The measured probe is not clean:
   finding 1911 found one dud bin moving the object's answer a whole bucket.
   The thresholds here are the measured behaviour of the current code, so a
   change that makes it more fragile shows up as a failure rather than as a
   number nobody compares.

4. A SINGLE CORRUPTED BIN -- a tone leaking into one probe slot, which is what
   a constant or intermittent interferer does.  This is the case the object
   handles worst, because two of its twenty-five bins ARE its whole input: if
   the interferer lands on the reference bin or the band edge, its answer moves
   by whole indices.  A fit over every bin should barely notice.

5. A NOTCH -- one bin sharply DOWN.  This is the case Phil rates as the more
   likely of the two, and it is the one the outlier rejection must NOT catch.
   The rejection is deliberately UPWARD ONLY: an interferer adds energy, a
   channel defect removes it, and a passive line cannot amplify.  A symmetric
   rule discarded the 3450 Hz band-edge cliff -- the single most important
   feature of this bench's channel -- and moved the answer from index 9 to 7.
   So this case asserts the opposite of case 4: the notch must SURVIVE.

THIS TESTS THE PYTHON MODEL, NOT THE C.  `preemphshape.py` and
`probe_preemp_shape()` in v34hshak.c were written from the same figures but not
from each other, and finding 1960 cross-checked them on a real capture: same
index, same bin count, residuals agreeing to 0.04 dB^2.  So this pins the
model the C implements.  A C-level test that drives `probeselect()` and reads
the index back out is the stronger thing and is task #164.
"""

import math
import random
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
# THE IMPLEMENTATION, not a copy of it.  An earlier version of this file
# reimplemented the scorer and so tested itself: a fix to preemphshape.py left
# it failing, because the two had drifted apart the moment they were written.
from preemphshape import (template_db, DE, BIN_HZ,           # noqa: E402
                          inband_bins, best_template, reject_outliers)

BAUDS = (2400, 2743, 3000, 3200, 3429)
EDGE = {2400: 18, 2743: 18, 3000: 19, 3200: 20, 3429: 22}   # 0-based


inband = inband_bins


def shape_pick(chan, baud, noise=0.0, rng=None, bump=None):
    """`chan(fs) -> dB`.  Optional per-bin noise and one corrupted bin."""
    fsm = dict(inband_bins(baud))

    def level(i):
        v = chan(fsm[i])
        if noise:
            v += rng.gauss(0.0, noise)
        if bump and bump[0] == i:
            v += bump[1]
        return v

    return best_template(level, baud)


def object_pick(chan, baud, noise=0.0, rng=None, bump=None):
    """The object's two-point counter, for contrast: bin 4 against the edge."""
    n = EDGE.get(baud, 22)

    def at(i):
        v = chan((i + 1) * BIN_HZ / baud)
        if noise:
            v += rng.gauss(0.0, noise)
        if bump and bump[0] == i:
            v += bump[1]
        return v

    ref, x, i = at(4), at(n), 5
    while True:
        x += 2.03                      # the multiplier is 2.03 dB in power
        i += 1
        if x > ref:
            return i
        if i > 9:
            return 10


FAILED = []


def check(name, got, want):
    ok = got == want
    if not ok:
        FAILED.append("%s: got %r want %r" % (name, got, want))
    print("  %-58s %s" % (name, "ok" if ok else "FAIL  got %r want %r"
                          % (got, want)))


def check_ge(name, got, floor):
    ok = got >= floor
    if not ok:
        FAILED.append("%s: %.3f below floor %.3f" % (name, got, floor))
    print("  %-58s %s (%.0f%%, floor %.0f%%)"
          % (name, "ok" if ok else "FAIL", 100 * got, 100 * floor))


def main():
    rng = random.Random(20260816)

    print("1. every template is reachable, and is the answer to its own inverse")
    for baud in BAUDS:
        got = [shape_pick(lambda fs, i=i: -template_db(i, fs), baud)
               for i in range(11)]
        check("identity, %d baud" % baud, got, list(range(11)))

    print("\n2. the object's counter on the same test (contrast, not a target)")
    for baud in (2400, 3429):
        got = [object_pick(lambda fs, i=i: -template_db(i, fs), baud)
               for i in range(11)]
        exact = sum(1 for i, g in enumerate(got) if g == i)
        print("  %-58s %d/11 exact, reachable set %s"
              % ("object, %d baud" % baud, exact, sorted(set(got))))
        # The point of the whole exercise: it cannot reach 0-5.
        check("object never returns an index below 6, %d baud" % baud,
              min(got) >= 6, True)

    print("\n3. broadband noise: fraction still exact, 3429 baud, 400 trials")
    for nsd, floor in ((0.25, 0.95), (0.50, 0.70), (1.00, 0.40)):
        ex = 0
        for _ in range(400):
            i = rng.randrange(11)
            ex += shape_pick(lambda fs, i=i: -template_db(i, fs),
                             3429, nsd, rng) == i
        check_ge("noise sd %.2f dB" % nsd, ex / 400.0, floor)

    print("\n4. one corrupted bin -- a tone leaking into a single probe slot")
    # +12 dB into one in-band bin, swept over every in-band bin, flat channel.
    for baud in (2400, 3429):
        bins = [i for i, _ in inband(baud)]
        worst_shape = worst_obj = 0
        for b in bins:
            s = shape_pick(lambda fs: 0.0, baud, bump=(b, 12.0))
            o = object_pick(lambda fs: 0.0, baud, bump=(b, 12.0))
            worst_shape = max(worst_shape, abs(s - 0))
            worst_obj = max(worst_obj, abs(o - 6))
        print("  %-58s shape %d, object %d"
              % ("worst index error from one +12 dB bin, %d baud" % baud,
                 worst_shape, worst_obj))
        # A fit over ~17 bins must not be moved more than one index by one of
        # them.  The object is allowed to be as bad as it is; this pins OURS.
        check("one bad bin moves the fit at most 1 index, %d baud" % baud,
              worst_shape <= 1, True)

    print("\n5. a notch -- one bin sharply DOWN -- must NOT be rejected")
    for baud in (2400, 3429):
        fsm = dict(inband_bins(baud))
        bins = [i for i, _ in inband_bins(baud)]
        # A deep notch on the highest in-band bin is a roll-off, and must pull
        # the answer UP the scale (more top-end correction), not be discarded.
        edge = max(bins)
        flat = shape_pick(lambda fs: 0.0, baud)
        notched = shape_pick(lambda fs: 0.0, baud, bump=(edge, -18.0))
        print("  %-58s flat %d -> notched %d"
              % ("deep notch on the top in-band bin, %d baud" % baud,
                 flat, notched))
        check("a notch changes the answer (it is not discarded), %d baud"
              % baud, notched != flat, True)
        # And a notch in the MIDDLE must not be thrown away either: assert the
        # bin survives rejection rather than asserting a particular index.
        mid = bins[len(bins) // 2]
        kept = reject_outliers([(i, -18.0 if i == mid else 0.0)
                                for i in bins])
        check("a mid-band notch survives outlier rejection, %d baud" % baud,
              any(i == mid for i, _ in kept), True)

    print("\n%s" % ("ALL PASS" if not FAILED else "FAILURES:"))
    for f in FAILED:
        print("  " + f)
    return 1 if FAILED else 0


if __name__ == "__main__":
    raise SystemExit(main())
