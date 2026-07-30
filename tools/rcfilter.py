#!/usr/bin/env python3
"""
Extract and characterise the FixedRC polyphase resampler banks.

Recovers the prototype FIR behind each of the 18 coefficient tables and fits it
to a windowed-sinc design, so the filters can be *redesigned* at another sample
rate rather than merely resampled.

Layout, established from RcFixed_Resample's address arithmetic
(.text 0x0b13df) -- see docs/coefficients.md:

  * a table is `up` polyphase branches of `taps` int16, phase-major:
    branch p occupies coeff[p*taps .. p*taps+taps-1]
  * the MAC walks coefficients forward against history oldest-to-newest, so
    coeff[0] multiplies the *oldest* sample -- the reverse of convolution
    order.  Hence:
        h[j*L + p] = coeff[p*N + (N-1-j)]
  * coefficients are Q14; the accumulator is shifted right by 14
  * the prototype is Type-I linear phase, symmetric about h[i] == h[L*N - i],
    i.e. length L*N+1 with the final tap dropped because it does not fit

Usage:
    rcfilter.py <obj>                 characterise every bank
    rcfilter.py <obj> --mode 3        one bank, with full detail
    rcfilter.py <obj> --md out.md     write a summary table
"""

import argparse
import cmath
import math
import struct
import subprocess

# mode -> (coefficient address, taps), from the jump table at .rodata 0x10f14
# and its initialiser stubs.  Modes 0 and 1 use a different state layout.
BANKS = {
    2:  (0x10c80, 36), 3:  (0x10b40, 32), 4:  (0x10c80, 36), 5:  (0x10aa0, 68),
    6:  (0x10b40, 32), 7:  (0x10a00, 68), 8:  (0x10880, 46), 9:  (0x107e0, 68),
    10: (0x10060, 40), 11: (0x0fda0, 68), 12: (0x0fc80, 28), 13: (0x0faa0, 58),
    14: (0x0f9a0, 42), 15: (0x0f8e0, 48), 16: (0x0f5e0, 38), 17: (0x0f440, 68),
    18: (0x0f140, 38), 19: (0x0ef00, 32),
}

DOWN = [1, 4, 5, 6, 1, 6, 1, 5, 1, 4, 5, 24, 4, 5, 2, 3, 3, 10, 9, 10]
UP = [4, 1, 6, 5, 6, 1, 5, 1, 4, 1, 24, 5, 5, 4, 3, 2, 10, 3, 10, 9]

# The design rate of a bank is L * f_in, where f_in is the *input* rate of
# the conversion -- NOT a fixed host rate.  Check_Combination(in, out) sets
# down = in/gcd and up = out/gcd, so out = in * up / down.
#
# This matters: the 9600 -> 8000 direction (mode 3, L=5) designs at
# 5 * 9600 = 48000, and the 8000 -> 9600 direction (mode 2, L=6) designs at
# 6 * 8000 = 48000.  Both land on the same design rate.  An earlier version of
# this tool assumed f_in was always 9600 and mis-reported mode 2 as 57600.
#
# So frequencies are reported normalised to the design rate; pass --fin to get
# absolute figures for a particular input rate.
DEFAULT_FIN = 9600.0


def rodata(obj):
    return subprocess.run(
        ["objcopy", "-O", "binary", "--only-section", ".rodata", obj, "/dev/stdout"],
        capture_output=True, check=True).stdout


def prototype(blob, addr, taps, up):
    """De-interleave a bank into its prototype FIR, longest form."""
    raw = list(struct.unpack_from("<%dh" % (taps * up), blob, addr))
    ph = [raw[p * taps:(p + 1) * taps] for p in range(up)]
    h = [ph[p][taps - 1 - j] for j in range(taps) for p in range(up)]
    # Type-I symmetric about index L*N/2; the final tap equals h[0] and was
    # dropped because L*N+1 does not fit the allocation.
    return h + [h[0]]


def sinc(x):
    return 1.0 if x == 0 else math.sin(math.pi * x) / (math.pi * x)


def bessel_i0(x):
    s = t = 1.0
    for k in range(1, 60):
        t *= (x / 2.0 / k) ** 2
        s += t
        if t < 1e-18 * s:
            break
    return s


def kaiser(n, m, beta):
    r = (2.0 * n / (m - 1)) - 1.0
    return bessel_i0(beta * math.sqrt(max(0.0, 1 - r * r))) / bessel_i0(beta)


def response_db(h, f, fs):
    w = 2 * math.pi * f / fs
    dc = abs(sum(h))
    if dc == 0:
        return -240.0
    m = abs(sum(h[n] * cmath.exp(-1j * w * n) for n in range(len(h)))) / dc
    return 20 * math.log10(m) if m > 1e-12 else -240.0


def binding_nyquist_norm(up, down):
    """Binding Nyquist as a fraction of the design rate fs = up * f_in.

    After interpolating by `up`, images must be suppressed above fs/(2*up),
    and the subsequent decimation by `down` requires fs/(2*down).  The
    stricter of the two binds.
    """
    return min(1.0 / (2.0 * up), 1.0 / (2.0 * down))


def measure(h, fs):
    """Passband edge (-0.5 dB), -6 dB point, and stopband floor."""
    nyq = fs / 2.0
    step = max(1.0, nyq / 2000.0)
    pb = m6 = None
    f = 0.0
    while f < nyq:
        d = response_db(h, f, fs)
        if pb is None and d <= -0.5:
            pb = f
        if m6 is None and d <= -6.0:
            m6 = f
            break
        f += step
    floor = -240.0
    if m6:
        g = m6 * 1.35
        while g < nyq:
            floor = max(floor, response_db(h, g, fs))
            g += step
    return pb, m6, floor


def fit_kaiser(h, fs):
    """Best windowed-sinc fit: returns (max_err, exact_count, beta, fc)."""
    m = len(h)
    c = (m - 1) / 2.0
    total = sum(h)
    best = None
    _pb, m6, _fl = measure(h, fs)
    if not m6:
        return None
    for bi in range(20, 1201):          # beta 0.2 .. 12.0
        beta = bi / 100.0
        for fi in range(-30, 31):       # fc within +-3% of the -6 dB point
            fc = m6 * (1.0 + fi * 0.001)
            base = [2 * fc / fs * sinc(2 * fc / fs * (n - c)) * kaiser(n, m, beta)
                    for n in range(m)]
            s = sum(base)
            if abs(s) < 1e-12:
                continue
            g = total / s
            t = [int(math.floor(g * b + 0.5)) for b in base]
            err = max(abs(t[n] - h[n]) for n in range(m))
            if best is None or err < best[0]:
                ex = sum(1 for n in range(m) if t[n] == h[n])
                best = (err, ex, beta, fc)
    return best


def analyse(blob, mode, fin=DEFAULT_FIN):
    addr, taps = BANKS[mode]
    up, down = UP[mode], DOWN[mode]
    h = prototype(blob, addr, taps, up)
    fs = up * fin
    sym = max(abs(h[i] - h[len(h) - 1 - i]) for i in range(len(h) // 2))
    pb, m6, floor = measure(h, fs)
    return {"mode": mode, "addr": addr, "taps": taps, "up": up, "down": down,
            "proto": h, "fs": fs, "sym_err": sym, "pb": pb, "m6": m6,
            "floor": floor, "fin": fin,
            "m6_norm": m6 / fs if m6 else None,
            "nyq_norm": binding_nyquist_norm(up, down),
            "ratio": (m6 / fs) / binding_nyquist_norm(up, down) if m6 else None}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("obj")
    ap.add_argument("--mode", type=int)
    ap.add_argument("--md")
    ap.add_argument("--fit", action="store_true",
                    help="also fit a Kaiser design (slow)")
    ap.add_argument("--fin", type=float, default=DEFAULT_FIN,
                    help="input rate, for absolute frequencies (default 9600)")
    args = ap.parse_args()

    blob = rodata(args.obj)
    modes = [args.mode] if args.mode else sorted(BANKS)

    rows = []
    for m in modes:
        a = analyse(blob, m, args.fin)
        fit = fit_kaiser(a["proto"], a["fs"]) if (args.fit or args.mode) else None
        rows.append((a, fit))
        print("mode %-2d  %2d:%-2d  0x%05x  %d taps x %d phases  proto=%d  "
              "sym_err=%d" % (m, a["down"], a["up"], a["addr"], a["taps"],
                              a["up"], len(a["proto"]), a["sym_err"]))
        print("        -6dB/fs=%.5f  binding Nyq/fs=%.5f  ratio=%.3f  "
              "stopband floor=%.1f dB"
              % (a["m6_norm"] or 0, a["nyq_norm"], a["ratio"] or 0, a["floor"]))
        print("        at f_in=%.0f: design fs=%.0f, -6dB=%s"
              % (a["fin"], a["fs"], "%.0f Hz" % a["m6"] if a["m6"] else "?"))
        if fit:
            print("        kaiser fit: beta=%.2f fc=%.0f Hz  max|err|=%d  exact=%d/%d"
                  % (fit[2], fit[3], fit[0], fit[1], len(a["proto"])))

    if args.md:
        with open(args.md, "w") as f:
            f.write("# FixedRC resampler banks\n\n")
            f.write("Generated by `tools/rcfilter.py`. Design rate is "
                    "`up * 9600 Hz`.\n\n")
            f.write("Frequencies are normalised to the design rate "
                    "`fs = up * f_in`, so they hold at any input rate.\n\n")
            f.write("| mode | down:up | addr | taps | phases | proto | sym err |"
                    " -6dB/fs | Nyq/fs | ratio | stopband |\n")
            f.write("|--:|--:|---|--:|--:|--:|--:|--:|--:|--:|--:|\n")
            for a, _fit in rows:
                f.write("| %d | %d:%d | `0x%05x` | %d | %d | %d | %d | %.5f | "
                        "%.5f | %.3f | %.1f dB |\n"
                        % (a["mode"], a["down"], a["up"], a["addr"], a["taps"],
                           a["up"], len(a["proto"]), a["sym_err"],
                           a["m6_norm"] or 0, a["nyq_norm"], a["ratio"] or 0,
                           a["floor"]))
        print("wrote %s" % args.md)


if __name__ == "__main__":
    main()
