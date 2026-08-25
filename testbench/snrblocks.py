#!/usr/bin/env python3
"""snrblocks.py -- the receiver's own SNR at the moment it chooses a rate.

    snrblocks.py captures/*.slmodemd.log > captures/snr.csv
    snrblocks.py --settled captures/foo.slmodemd.log     # per-block trace

WHY A SECOND TOOL AND NOT A FLAG ON abextract.py.  abextract emits ONE row per
call, anchored on `pty CONNECT nnn`, because the pre-emphasis A/B compared two
arms a call at a time.  The question here is different: it is whether the
number the ladder read was a fair sample of the channel, and that is a property
of a DECISION BLOCK, not of a call.  A call holds up to five of them and each
one carries its own `ethreh`, its own printed thresholds and its own
`finally rxbitrate` -- so a block is internally self-consistent even when the
call-level pairing is ambiguous, which is the trap abextract's comment
describes and works around.

THE SNR ITSELF.  `V34EQUPOW` prints `f248` beside `f21a` from the same
1024-symbol block of v34rx.c's error accumulator:

    equerr = f21a = (SUM |decision - equaliser out|^2) >> 16
    sigpow = f248 = (SUM (|decision|^2 >> 8)) >> 8

Both are the same sum over the same 1024 symbols scaled by 1/65536, so their
ratio is dimensionless and 10*log10(sigpow/equerr) is a slicer SNR in dB.  The
ladder's thresholds are in `equerr`'s units too (v34hstx1.cpp:2624), so the
same constant converts a threshold into the SNR that rate demands.

TWO THINGS THAT WOULD MAKE A ROW A LIE, both filtered here rather than
averaged over:

  * `equerr` saturates at 32767 and `sigpow` does not, so a saturated block
    reads as a floor of 10*log10(sigpow/32767) ~ 7 dB whatever the channel
    really was.  Marked `sat`, never averaged.
  * `sigpow` is the power of the DECISION, so in the settled period it is a
    constellation constant -- pinned to 163818-163823 across every call in the
    archive.  A block whose sigpow is off that value is slicing against
    something else (training constellation, or a transition straddled
    mid-block) and is a different regime.  Marked `regime`.
"""

import csv
import math
import os
import re
import sys

# The pinned settled-period value, and the tolerance that separates the
# settled constellation from every other regime.  Measured, not chosen: the
# archive's settled blocks span 163818-163823 on a value of 163820, which is
# 0.003%, so 0.5% is two orders of margin and still excludes the 166000-174000
# training regime by a wide gap.
SIGPOW_SETTLED = 163820
SIGPOW_TOL = 0.005

EQUERR_SAT = 32767

RE_EQUPOW = re.compile(r"V34EQUPOW, sigpow = (-?\d+), equerr = (-?\d+)")
# THE PRE-INSTRUMENT ARCHIVE, readable because `sigpow` turned out to be a
# constant (finding F3200).  A capture with no `V34EQUPOW` at all still carries
# the object's own `V34EQU`, and 163820 is the value `sigpow` takes on every
# settled block ever logged -- so the same SNR comes out with a fixed 52.14 dB
# offset.  Used ONLY when the log has no V34EQUPOW anywhere: never mixed with
# measured values inside one call, and every row it produces is flagged
# `assumed` so a reader can tell which archive a number came from.
RE_EQU = re.compile(r"V34EQU, equerr = (-?\d+), preerr = (-?\d+)")
RE_THRESH = re.compile(r"V34DATARATE,threshold for data rate (\d+) = (-?\d+)")
RE_ETHRESH = re.compile(r"V34DATARATE, ethresh data rate = (-?\d+),ethreh=(-?\d+)")
RE_DECIDE = re.compile(r"V34DATARATE, equerr = (\d+),preerr=(\d+)")
RE_FINAL = re.compile(r"V34DATARATE, finally txbitrate (\d+),rxbitrate (\d+)")
RE_CHOICE = re.compile(r"V34DATARATE, Final choice data rate = (-?\d+)")
RE_TS = re.compile(r"^<(\d+\.\d+)>")

# THE PHASE 3 / PHASE 4 BOUNDARY, and the gain on each side of it.
#
# A V.34 startup trains the receiver TWICE against the same channel: once in
# phase 3, when we are receiving and transmitting nothing, and again in phase
# 4, when we are doing both at once.  The rate is decided from the second one.
# The first is therefore a control the call performs on itself -- same modems,
# same codec hop, seconds apart -- and any difference between the two is ours
# and not the line's.
#
# `Agc gain estimate at the end of phase 3` is the marker between them, and
# the value it prints is the gain the phase 4 receiver is then set up with
# (the following `setup receiver gain` echoes it).  So one handshake yields a
# phase 3 gain, a phase 4 gain, a phase 3 SNR and a decision SNR.
RE_AGC_SETUP = re.compile(r"V34AGC, setup receiver gain = 0x([0-9a-fA-F]+)")
RE_P3_END = re.compile(r"Agc gain estimate at the end of phase 3 is (\d+)")


def db(sig, err):
    """dB, or None where the ratio is not defined."""
    if not sig or not err or sig <= 0 or err <= 0:
        return None
    return 10.0 * math.log10(float(sig) / float(err))


def read(path):
    try:
        return open(path, "rb").read().decode("latin-1")
    except OSError:
        return ""


def farend(base):
    """The far modem's own bookkeeping, in either of the two dialects.

    Returned as (what it RECEIVED, what it TRANSMITTED) -- so the second
    number is the one our receiver had to demodulate.  The USR Courier's
    ATI11 says `Speed recv/xmit`; the Rockwell and Conexant AT&V1 spell it
    out over two lines and in the opposite order.
    """
    t = read(base + ".lastlink.log")
    if not t:
        return "", ""
    m = re.search(r"Speed\s+(\d+)/(\d+)", t)
    if m:
        return m.group(1), m.group(2)
    tx = re.search(r"LAST TX rate\.+ *(\d+)", t)
    rx = re.search(r"LAST RX rate\.+ *(\d+)", t)
    if tx or rx:
        return (rx.group(1) if rx else ""), (tx.group(1) if tx else "")
    return "", ""


def powline(sl):
    """(regex, assumed) -- how this log reports the equaliser's error."""
    if "V34EQUPOW" in sl:
        return RE_EQUPOW, False
    return RE_EQU, True


def blocks(sl):
    """Every rate decision in one log, with the sample it was taken on."""
    rx_pow, assumed = powline(sl)
    out = []
    last_pow = None          # (sigpow, equerr) of the most recent V34EQUPOW
    cur = None
    agc = None               # most recent `setup receiver gain`
    agc_p3 = agc_p4 = None
    in_p4 = False
    p3_run = []              # settled-constellation SNRs seen in this phase 3
    p4_run = []              # (equerr, dB) seen since the phase 4 gain was set
    p3_db = None
    p3_n = 0                 # how many settled blocks the control averaged over
    for line in sl.splitlines():
        m = RE_AGC_SETUP.search(line)
        if m:
            agc = int(m.group(1), 16)
            if in_p4:
                agc_p4, p4_run = agc, []
                in_p4 = False
            else:
                # A NEW HANDSHAKE STARTS HERE, so the previous one's phase 3
                # is over whether or not a decision followed it.  Clearing
                # `agc_p4` is what re-opens phase 3 collection: without it
                # only the FIRST handshake of a call would ever get a control,
                # and retrains are where the within-call comparison lives.
                agc_p3, p3_run, agc_p4, p3_db = agc, [], None, None
                p3_n = 0
                p4_run = []
            continue

        m = RE_P3_END.search(line)
        if m:
            # The last few blocks, not all of them: phase 3 starts with the
            # equaliser unconverged and one saturated block, and averaging
            # that in would understate a measurement whose whole point is to
            # be the clean control.
            tail = [v for v in p3_run[-3:]]
            p3_db = sum(tail) / len(tail) if tail else None
            # REPORTED, not just used: a control averaged over one block is a
            # different measurement from one averaged over three, and pooling
            # far ends that differ here is how a 10 dB discrepancy hides.
            p3_n = len(p3_run)
            in_p4 = True
            continue

        m = rx_pow.search(line)
        if m:
            last_pow = ((SIGPOW_SETTLED, int(m.group(1))) if assumed
                        else (int(m.group(1)), int(m.group(2))))
            s, e = last_pow
            d = db(s, e)
            clean = (d is not None and e < EQUERR_SAT
                     and SIGPOW_SETTLED * (1 - SIGPOW_TOL) <= s
                     <= SIGPOW_SETTLED * (1 + SIGPOW_TOL))
            if clean:
                if agc_p4 is None and not in_p4:
                    p3_run.append(d)
                elif agc_p4 is not None:
                    p4_run.append((e, d))
            if cur is not None and cur.get("after_pow") is None:
                cur["after_pow"] = last_pow
            continue

        m = RE_THRESH.search(line)
        if m:
            if cur is None:
                cur = {"thresh": {}, "after_pow": None}
            cur["thresh"][int(m.group(1))] = int(m.group(2))
            continue

        m = RE_ETHRESH.search(line)
        if m and cur is not None:
            cur["ladder_rate"] = int(m.group(1))
            cur["ethreh"] = int(m.group(2))
            ts = RE_TS.match(line)
            cur["t"] = float(ts.group(1)) if ts else None
            cur["agc_p3"], cur["agc_p4"], cur["p3_db"] = agc_p3, agc_p4, p3_db
            cur["p3_n"] = p3_n
            cur["p4_run"] = list(p4_run)
            continue

        m = RE_DECIDE.search(line)
        if m and cur is not None:
            cur["equerr"] = int(m.group(1))
            cur["preerr"] = int(m.group(2))
            # PAIR BY VALUE, NOT BY POSITION.  The decision reads f21a, which
            # is exactly what the last V34EQU block published, so an equal
            # value is a confirmed pairing and an unequal one is a gap in the
            # log rather than something to paper over with "near enough".
            cur["sigpow"] = (last_pow[0] if last_pow
                             and last_pow[1] == cur["equerr"] else None)
            cur["assumed"] = assumed
            continue

        m = RE_CHOICE.search(line)
        if m and cur is not None:
            cur["choice"] = int(m.group(1))
            continue

        m = RE_FINAL.search(line)
        if m and cur is not None:
            cur["txbits"] = int(m.group(1))
            cur["rxbits"] = int(m.group(2))
            out.append(cur)
            cur = None
    if cur is not None and "ethreh" in cur:
        out.append(cur)
    return out


def trace(sl):
    """Every error block, in order: (t, sigpow, equerr)."""
    rx_pow, assumed = powline(sl)
    out = []
    for line in sl.splitlines():
        m = rx_pow.search(line)
        if m:
            if assumed:
                ts = RE_TS.match(line)
                out.append((float(ts.group(1)) if ts else None,
                            SIGPOW_SETTLED, int(m.group(1))))
                continue
            ts = RE_TS.match(line)
            out.append((float(ts.group(1)) if ts else None,
                        int(m.group(1)), int(m.group(2))))
    return out


def settled(tr, after=None):
    """Median SNR over settled-constellation blocks, and how many there were.

    The median rather than the mean: one saturated or one straddling block
    would drag a mean of twenty by several dB, and the question this answers
    is what the channel typically delivered, not what its worst block did.
    """
    lo = SIGPOW_SETTLED * (1.0 - SIGPOW_TOL)
    hi = SIGPOW_SETTLED * (1.0 + SIGPOW_TOL)
    vals = [db(s, e) for (t, s, e) in tr
            if lo <= s <= hi and e < EQUERR_SAT
            and (after is None or t is None or t >= after)]
    vals = sorted(v for v in vals if v is not None)
    if not vals:
        return None, 0
    n = len(vals)
    return (vals[n // 2] if n % 2 else 0.5 * (vals[n // 2 - 1] + vals[n // 2])), n


def logs_for(arg):
    """Accept a log path or a capture basename."""
    for suf in (".slmodemd.log", ".answer.log", ".origin.log"):
        if arg.endswith(suf):
            return [(arg[:-len(suf)], arg)]
    found = []
    for suf in (".slmodemd.log", ".answer.log", ".origin.log"):
        if os.path.exists(arg + suf):
            found.append((arg, arg + suf))
    return found


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    want_trace = "--settled" in sys.argv[1:]

    if want_trace:
        for arg in args:
            for base, path in logs_for(arg):
                tr = trace(read(path))
                print("# %s -- %d blocks" % (os.path.basename(base), len(tr)))
                for (t, s, e) in tr:
                    d = db(s, e)
                    tag = ""
                    if e >= EQUERR_SAT:
                        tag = " SAT"
                    elif not (SIGPOW_SETTLED * (1 - SIGPOW_TOL) <= s
                              <= SIGPOW_SETTLED * (1 + SIGPOW_TOL)):
                        tag = " regime"
                    print("  %10.3f  sigpow %7d  equerr %6d  %s%s"
                          % (t if t is not None else 0.0, s, e,
                             "%6.2f dB" % d if d is not None else "    -   ",
                             tag))
        return 0

    w = csv.writer(sys.stdout)
    # TWO SNRs, because two different questions.  `snr_db` is off `ethreh`,
    # which is the number the ladder actually compared -- so it answers "was
    # the mapping applied correctly".  `snr_eq_db` is off `equerr`, the raw
    # equaliser error before the predictor is credited -- so it answers "what
    # did the demodulator achieve".  They differ whenever the predictor was
    # kept, which is the `preerr` path at v34hstx1.cpp:2526.
    w.writerow(["call", "block", "t", "ethreh", "equerr", "preerr", "sigpow",
                "snr_db", "snr_eq_db", "flag", "ladder_rate", "choice",
                "rxbits", "txbits",
                "thresh_db_for_chosen", "p3_db", "p3_n", "p3_minus_dec",
                "p4_best_db", "spike_db", "p4_n",
                "agc_p3", "agc_p4", "settled_db", "settled_n",
                "far_recv", "far_xmit"])
    for arg in sorted(args):
        for base, path in logs_for(arg):
            sl = read(path)
            tr = trace(sl)
            bs = blocks(sl)
            far_rx, far_tx = farend(base)
            for i, b in enumerate(bs):
                sp = b.get("sigpow")
                eth = b.get("ethreh")
                d = db(sp, eth)
                deq = db(sp, b.get("equerr"))
                flag = ""
                if eth is not None and eth >= EQUERR_SAT:
                    flag = "sat"
                elif sp is None:
                    flag = "unpaired"
                elif not (SIGPOW_SETTLED * (1 - SIGPOW_TOL) <= sp
                          <= SIGPOW_SETTLED * (1 + SIGPOW_TOL)):
                    flag = "regime"
                if b.get("assumed") and flag != "sat":
                    flag = (flag + "+assumed") if flag else "assumed"
                # What the rate it settled on was asking for, in the same dB.
                ch = b.get("choice")
                tdb = None
                if sp and ch is not None and ch in b.get("thresh", {}):
                    tdb = db(sp, b["thresh"][ch])
                # THE SAME TRAINING RUN, WITHOUT THE BLOCK THE LADDER READ.
                # Excluded by value rather than by position: the decision
                # reports the equerr of its own block, and dropping "the last
                # one" would drop a different block whenever another lands
                # between the accumulator and the printf.
                run = [v for (e, v) in b.get("p4_run", [])
                       if e != b.get("equerr")]
                pbest = max(run) if run else None
                sdb, sn = settled(tr, after=b.get("t"))
                w.writerow([os.path.basename(base), i,
                            "%.3f" % b["t"] if b.get("t") else "",
                            eth, b.get("equerr"), b.get("preerr"), sp,
                            "%.2f" % d if d is not None else "",
                            "%.2f" % deq if deq is not None else "",
                            flag, b.get("ladder_rate"), ch,
                            b.get("rxbits"), b.get("txbits"),
                            "%.2f" % tdb if tdb is not None else "",
                            "%.2f" % b["p3_db"] if b.get("p3_db") else "",
                            b.get("p3_n", 0),
                            "%.2f" % (b["p3_db"] - d)
                            if b.get("p3_db") and d is not None else "",
                            "%.2f" % pbest if pbest is not None else "",
                            "%.2f" % (pbest - d)
                            if pbest is not None and d is not None else "",
                            len(run),
                            b.get("agc_p3"), b.get("agc_p4"),
                            "%.2f" % sdb if sdb is not None else "", sn,
                            far_rx, far_tx])
    return 0


if __name__ == "__main__":
    sys.exit(main())
