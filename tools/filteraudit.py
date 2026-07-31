#!/usr/bin/env python3
"""
Audit the resampler filter-design tooling.

`rcfilter.py` recovers each bank's prototype and fits a windowed sinc to it,
reporting the fit as a maximum coefficient error in LSB.  That number has been
carried around as if it meant something, and this asks whether it does.

Four questions, each answered with a measurement rather than an opinion:

  1. COST.  The fit is off by a few LSB.  What is that worth in dB of
     frequency response?  A filter is judged by its response; coefficient
     distance is a proxy that nobody actually cares about.

  2. ROUNDING.  The fit rounds half-up.  The library's other tables --
     sine, sqrt, div -- all TRUNCATE.  Does truncation, or round-half-even,
     fit better?

  3. NORMALISATION.  The fit scales the whole prototype so its sum matches.
     A polyphase filter is used one branch at a time, so a designer might
     normalise each branch instead.  Does that fit better?

  4. OBJECTIVE.  Fitting minimises max coefficient error.  Does minimising
     response error instead pick a different design, and is that design
     better?

Usage:
    filteraudit.py <obj>              audit the default modes
    filteraudit.py <obj> --mode 3     one mode
"""

import argparse
import math
import sys

sys.path.insert(0, __file__.rsplit('/', 1)[0])
import rcfilter


def response_db(h, f, fs):
    return rcfilter.response_db(h, f, fs)


def quality(h, fs, m6):
    """Passband ripple and stopband floor -- what a designer actually reads.

    The stopband starts at 1.35x the -6 dB point, the same allowance
    rcfilter.measure uses, so the numbers are comparable with its output.
    Measuring from the binding Nyquist instead puts the transition band in
    the average and reports about -20 dB for every filter here.
    """
    pb_hi = m6 * 0.85
    lo = [response_db(h, pb_hi * i / 60.0, fs) for i in range(61)]
    ripple = max(lo) - min(lo)
    sb = m6 * 1.35
    floor = max(response_db(h, sb + (fs / 2.0 - sb) * i / 500.0, fs)
                for i in range(501))
    return ripple, floor


def exact_design(m, fs, fc, beta, gain):
    """The same design, unquantised -- separates design from quantisation."""
    c = (m - 1) / 2.0
    base = [2 * fc / fs * rcfilter.sinc(2 * fc / fs * (n - c))
            * rcfilter.kaiser(n, m, beta) for n in range(m)]
    g = gain / sum(base)
    return [g * b for b in base]


def response_error(a, b, fs, lo, hi, n=400):
    """Worst |A(f) - B(f)| in dB over [lo, hi], ignoring nulls."""
    worst = 0.0
    at = lo
    for i in range(n + 1):
        f = lo + (hi - lo) * i / n
        da = response_db(a, f, fs)
        db = response_db(b, f, fs)
        # Below -120 dB both are numerically noise; comparing them is
        # comparing rounding, not filters.
        if da < -120.0 and db < -120.0:
            continue
        if abs(da - db) > worst:
            worst = abs(da - db)
            at = f
    return worst, at


def design(m, fs, fc, beta, gain, rounding):
    c = (m - 1) / 2.0
    base = [2 * fc / fs * rcfilter.sinc(2 * fc / fs * (n - c))
            * rcfilter.kaiser(n, m, beta) for n in range(m)]
    s = sum(base)
    if abs(s) < 1e-12:
        return None
    g = gain / s
    out = []
    for b in base:
        v = g * b
        if rounding == 'half-up':
            out.append(int(math.floor(v + 0.5)))
        elif rounding == 'trunc':
            out.append(int(v))
        elif rounding == 'floor':
            out.append(int(math.floor(v)))
        elif rounding == 'half-even':
            out.append(int(round(v)))
        else:
            raise ValueError(rounding)
    return out


def search(h, fs, rounding, objective, coarse=True):
    """Best (err, beta, fc) under one rounding mode and one objective."""
    m = len(h)
    total = sum(h)
    _pb, m6, _fl = rcfilter.measure(h, fs)
    if not m6:
        return None
    best = None
    bstep = 5 if coarse else 1
    for bi in range(20, 1201, bstep):
        beta = bi / 100.0
        for fi in range(-30, 31, 1 if coarse else 1):
            fc = m6 * (1.0 + fi * 0.001)
            t = design(m, fs, fc, beta, total, rounding)
            if t is None:
                continue
            if objective == 'coeff':
                err = max(abs(t[n] - h[n]) for n in range(m))
            else:
                err, _ = response_error(h, t, fs, 0.0, fs / 2.0, 120)
            if best is None or err < best[0]:
                best = (err, beta, fc, t)
    return best


def total_of(h):
    return sum(h)


def audit_mode(blob, mode):
    a = rcfilter.analyse(blob, mode)
    h = a["proto"]
    fs = a["fs"]
    m = len(h)
    print("=" * 72)
    print("mode %d: %d taps, prototype %d, fs %.0f" % (mode, a["taps"], m, fs))

    base = search(h, fs, 'half-up', 'coeff')
    err, beta, fc, fit = base
    exact = sum(1 for n in range(m) if fit[n] == h[n])
    print("\n  current tooling (half-up rounding, coefficient objective)")
    print("    beta=%.2f fc=%.1f  max|err|=%d LSB  exact=%d/%d"
          % (beta, fc, err, exact, m))

    # 1. What does that cost, in the numbers a designer reads?
    _pb, m6, _fl = rcfilter.measure(h, fs)
    print("\n  1. COST of that %d LSB, as ripple and stopband:" % err)
    print("     %-30s %10s %11s" % ("", "pb ripple", "stopband"))
    for label, g in (("ORIGINAL", h),
                     ("fitted, quantised to Q14", fit),
                     ("fitted, unquantised",
                      exact_design(m, fs, fc, beta, total_of(h)))):
        r, f = quality(g, fs, m6)
        print("     %-30s %8.4f dB %8.1f dB" % (label, r, f))
    print("     -- the gap between the last two rows is what 14-bit")
    print("        quantisation costs, and it dwarfs the fit residual.")

    # 2. Rounding modes.
    print("\n  2. ROUNDING mode (coefficient objective):")
    for mode_name in ('half-up', 'trunc', 'floor', 'half-even'):
        r = search(h, fs, mode_name, 'coeff')
        ex = sum(1 for n in range(m) if r[3][n] == h[n])
        print("     %-10s max|err|=%2d LSB  exact=%3d/%d  beta=%.2f"
              % (mode_name, r[0], ex, m, r[1]))

    # 3. Per-branch normalisation: does each polyphase branch sum alike?
    up = a["up"]
    taps = a["taps"]
    print("\n  3. NORMALISATION -- per-branch sums of the ORIGINAL:")
    sums = []
    for p in range(up):
        s = sum(h[j * up + p] for j in range((m - p + up - 1) // up))
        sums.append(s)
    print("     branch sums: %s" % sums)
    print("     spread %d, mean %.1f -- %s"
          % (max(sums) - min(sums), sum(sums) / float(len(sums)),
             "equal, so per-branch normalisation is indistinguishable"
             if max(sums) - min(sums) <= 1 else
             "UNEQUAL, so the designer did not normalise per branch"))

    # 4. Objective.
    print("\n  4. OBJECTIVE -- minimise response error instead:")
    r = search(h, fs, 'half-up', 'response')
    cerr = max(abs(r[3][n] - h[n]) for n in range(m))
    for label, g, e in (("coefficient-fitted", fit, err),
                        ("response-fitted", r[3], cerr)):
        rip, fl = quality(g, fs, m6)
        rip2, fl2 = quality(exact_design(m, fs,
                                         fc if label[0] == 'c' else r[2],
                                         beta if label[0] == 'c' else r[1],
                                         total_of(h)), fs, m6)
        print("     %-20s Q14 %8.4f dB %7.1f dB | exact %7.1f dB | %4d LSB"
              % (label, rip, fl, fl2, e))
    print("     -- coefficient fitting REPRODUCES; response fitting DESIGNS.")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("obj")
    ap.add_argument("--mode", type=int, action="append")
    args = ap.parse_args()

    blob = rcfilter.rodata(args.obj)
    modes = args.mode if args.mode else [2, 3]
    for m in modes:
        audit_mode(blob, m)


if __name__ == "__main__":
    main()
