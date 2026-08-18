#!/usr/bin/env python3
"""
linesweep.py -- measure, store, model and compare the line response under
different ATA impedance settings, per modem.

    linesweep.py ingest  --impedance 600r     --modem courier  captures/hs-courier-*.slmodemd.log
    linesweep.py measure --impedance complex2 --modem courier --calls 10
    linesweep.py list
    linesweep.py compare
    linesweep.py plot -o linemodels/compare.png

WHY THIS EXISTS
---------------
The bench's Cisco VG204 terminates each analogue port with a selectable
impedance -- `600r` (the platform default, and what every capture before
2026-08-16 was taken at), `complex1` (the BT/UK network impedance) and
`complex2` (the ETSI one).  That setting is the hybrid's balance network, so it
moves trans-hybrid loss, the reflected frequency response, and level transfer.

AND THE RIGHT SETTING IS PROBABLY NOT THE SAME FOR EVERY MODEM.  The Courier
reports `Product type: UK External MSK` to ATI7, so its front end was built
against BT line impedance; the Oli'Net and the SupraExpress are different
designs of different vintage and market.  A setting that improves one may make
another worse, and the only way to know is to measure each modem under each
setting and keep every arm.  That is what this tool is for.

WHAT IT MEASURES
----------------
The V.34 line probe (V.34 sec 11.2 / Table 17): 25 tones 150 Hz apart from 150
to 3750 Hz, emitted BEFORE the far end applies pre-emphasis, which finding 1907
records as the only safe window for a channel measurement.  Reduction is
`bandshape.py` -- this tool does not re-implement it, it imports it, so an arm
measured today and an arm measured next month go through identical code.  A
comparison whose two sides went through different reduction is not a
comparison.

THE HONESTY RULES THIS TOOL ENFORCES
------------------------------------
1. THE IMPEDANCE IS OPERATOR-ASSERTED.  Nothing here can read the ATA.  If you
   tell it `--impedance complex1` while the device is on `600r`, it will
   believe you and the stored model will be a lie that outlives everyone's
   memory of the session.  Pass `--ata-config PATH` to a config dump and the
   tool records the file's sha256 AND greps the actual `impedance` lines out of
   it, which turns the assertion into evidence.  `list` marks every model as
   ASSERTED or EVIDENCED so you can see at a glance which is which.

2. DENOMINATORS ON EVERYTHING.  Calls, probes, and how many bins actually
   carried a reading.  A response averaged over three probes and one averaged
   over three hundred are different objects and must not print the same way.

3. FLOORED BINS ARE NOT READINGS.  `bandshape` reports the fraction of probes
   in which a bin sat on the noise floor.  Above `FLOOR_MAX` that bin is stored
   as null, not as its floor value, and plots break the line rather than
   drawing through it.  A floor is the instrument running out, not a
   measurement of the channel.

4. NOTHING IS EXTRAPOLATED.  Bins with no data stay absent.  No curve fitting
   beyond the measured points, no filling in the ends.

5. IT REFUSES TO REPORT ON ZERO.  A tool that prints a clean empty table is
   indistinguishable from a broken one (findings 134, 2400, 3100).

STORAGE
-------
One JSON per (impedance, modem, date) under `linemodels/`, schema 1, carrying
the bins, the tilt, the denominators, the provenance and the capture labels it
was reduced from.  Re-running `ingest` for the same key overwrites only that
arm.  The files are plain JSON on purpose: they outlive this tool.

RELATION TO THE EMULATOR
------------------------
`chanshim.py` carries selectable curves under `CHAN_LINE_MODEL`.  This tool
prints a ready-to-paste stanza (`compare --emit-chanshim`) so a measured arm
can become an emulator model without anybody retyping numbers.  It does NOT
edit `chanshim.py` -- the default there is byte-identical to finding 1907's fit
and every archived emulator result depends on it staying that way.
"""

import argparse
import datetime as _dt
import glob
import hashlib
import json
import os
import re
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bandshape                                          # noqa: E402

BENCH = os.path.dirname(os.path.abspath(__file__))
STORE = os.path.join(BENCH, "linemodels")
SCHEMA = 1
FLOOR_MAX = 0.5          # a bin floored in more than half the probes is null
TILT_BAND = (450.0, 3150.0)   # finding 1956's band; see compare() for why


# ----------------------------------------------------------------- provenance

def ata_evidence(path):
    """sha256 + the actual impedance lines from a VG204 config dump."""
    with open(path, "rb") as fh:
        blob = fh.read()
    text = blob.decode("utf-8", "replace")
    lines = sorted({l.strip() for l in text.splitlines()
                    if re.match(r"\s*impedance\s+\S+", l)})
    return {"path": os.path.abspath(path),
            "sha256": hashlib.sha256(blob).hexdigest(),
            "impedance_lines": lines,
            "ports_setting_impedance": len(
                [l for l in text.splitlines()
                 if re.match(r"\s*impedance\s+\S+", l)])}


def modem_identity(role):
    """Ask modems.sh, without opening the port if we can avoid it."""
    try:
        out = subprocess.run(
            ["bash", "-c",
             f'. "{BENCH}/modems.sh" 2>/dev/null && modem_provenance {role}'],
            capture_output=True, text=True, timeout=25)
        s = (out.stdout or "").strip()
        return s if s else None
    except Exception:
        return None


# -------------------------------------------------------------------- reduce

def build_model(impedance, modem, logs, ata_config=None, note=None):
    logs = [p for pat in logs for p in sorted(glob.glob(pat))] or []
    if not logs:
        sys.exit("linesweep: no logs matched -- refusing to build a model "
                 "from nothing (findings 134, 2400).")
    r = bandshape.report(f"{impedance}/{modem}", logs)
    if not r or not r.get("n_probe"):
        sys.exit(f"linesweep: {len(logs)} logs matched but they carry 0 line "
                 "probes. That is a broken input, not an empty channel.")

    bins = []
    for i, hz in enumerate(np.asarray(r["f"], float)):
        ff = float(r["ffrac"][i])
        floored = ff > FLOOR_MAX
        bins.append({
            "hz": float(hz),
            "db": (None if floored else float(r["med"][i])),
            "floored_frac": ff,
            "is_reading": not floored,
            "is_noise_bin": i in getattr(bandshape, "NOISE_BINS", set()),
        })

    # TILT COMES FROM bandshape.tilt(), NOT FROM AN ENDPOINT SUBTRACTION.
    # The first version of this subtracted the band's end bins and reported
    # -3.43 dB where bandshape's least-squares slope over the same data says
    # -3.72.  Two numbers for one quantity, from one tool, differing by 0.3 dB
    # -- exactly the defect this file's docstring warns about two paragraphs
    # up.  bandshape excludes floored and noise bins and fits rather than
    # subtracting, and it is the tool every other arm went through.
    lo, hi = TILT_BAND
    tdb, tn = bandshape.tilt(np.asarray(r["f"], float),
                             np.asarray(r["med"], float),
                             np.asarray(r["ffrac"], float), lo, hi)
    tilt = None if tdb is None else round(float(tdb), 3)
    pts = [0] * tn

    return {
        "schema": SCHEMA,
        "impedance": {
            "asserted": impedance,
            "evidence": ata_evidence(ata_config) if ata_config else None,
        },
        "modem": {"role": modem, "identity": modem_identity(modem)},
        "measured_utc": _dt.datetime.now(_dt.timezone.utc)
                            .strftime("%Y-%m-%dT%H:%M:%SZ"),
        "n_calls": int(r.get("n_logs") or len(logs)),
        "n_probes": int(r["n_probe"]),
        "n_bins_read": sum(1 for b in bins if b["is_reading"]),
        "n_bins_total": len(bins),
        "tilt": {"band_hz": [lo, hi], "db": tilt,
                 "n_points": len(pts)} if tilt is not None else None,
        "note": note,
        "captures": [os.path.basename(p).split(".")[0] for p in logs],
        "bins": bins,
    }


def key_of(m):
    return f'{m["impedance"]["asserted"]}__{m["modem"]["role"]}'


def save(m):
    os.makedirs(STORE, exist_ok=True)
    p = os.path.join(STORE, key_of(m) + ".json")
    with open(p, "w") as fh:
        json.dump(m, fh, indent=1, sort_keys=True)
    return p


def load_all():
    return sorted((json.load(open(p)) for p in
                   glob.glob(os.path.join(STORE, "*.json"))),
                  key=lambda m: (m["modem"]["role"],
                                 m["impedance"]["asserted"]))


# ------------------------------------------------------------------ commands

def cmd_ingest(a):
    m = build_model(a.impedance, a.modem, a.logs, a.ata_config, a.note)
    p = save(m)
    ev = "EVIDENCED" if m["impedance"]["evidence"] else "ASSERTED"
    print(f'stored {os.path.relpath(p, BENCH)}')
    print(f'  impedance {m["impedance"]["asserted"]} ({ev})   '
          f'modem {m["modem"]["role"]}')
    print(f'  {m["n_calls"]} calls, {m["n_probes"]} probes, '
          f'{m["n_bins_read"]}/{m["n_bins_total"]} bins carried a reading')
    print(f'  tilt {TILT_BAND[0]:.0f}-{TILT_BAND[1]:.0f} Hz: '
          f'{m["tilt"]["db"]:+.2f} dB' if m["tilt"] else '  tilt: not computable')
    if not m["impedance"]["evidence"]:
        print('  NOTE: impedance is your word for it. Pass --ata-config to a '
              'config dump\n        to record the sha256 and the actual '
              'impedance lines.')
    return 0


def cmd_measure(a):
    row = os.path.join(BENCH, "row.sh")
    ext = subprocess.run(["bash", "-c",
                          f'. "{BENCH}/modems.sh" && modem_ext {a.modem}'],
                         capture_output=True, text=True).stdout.strip()
    if not ext:
        sys.exit(f"linesweep: cannot resolve an extension for '{a.modem}'")
    stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M")
    labels = []
    print(f'measuring {a.modem} (ext {ext}) at impedance '
          f'{a.impedance} -- {a.calls} calls')
    for i in range(1, a.calls + 1):
        lab = f"ls-{a.impedance}-{a.modem}-{stamp}-{i}"
        subprocess.run([os.path.join(BENCH, "waitquiet.sh")], check=False)
        r = subprocess.run(["timeout", "220", row,
                            os.path.join(BENCH, "captures", lab), "pty", ext],
                           capture_output=True, text=True)
        ok = r.returncode == 0
        print(f'  {i}/{a.calls} {lab} {"ok" if ok else "FAILED rc=%d" % r.returncode}')
        if ok:
            labels.append(lab)
    print(f'  {len(labels)} of {a.calls} calls completed')
    if not labels:
        sys.exit("linesweep: no call completed; nothing to model.")
    a.logs = [os.path.join(BENCH, "captures", l + ".slmodemd.log")
              for l in labels]
    return cmd_ingest(a)


def cmd_list(a):
    ms = load_all()
    if not ms:
        sys.exit(f"linesweep: no models in {STORE}. Nothing to list.")
    print(f'{len(ms)} model(s) in {os.path.relpath(STORE, BENCH)}\n')
    print(f'  {"impedance":<10} {"modem":<9} {"calls":>5} {"probes":>7} '
          f'{"bins":>7} {"tilt dB":>8}  provenance   measured')
    for m in ms:
        t = m["tilt"]["db"] if m["tilt"] else float("nan")
        ev = "EVIDENCED" if m["impedance"]["evidence"] else "asserted"
        print(f'  {m["impedance"]["asserted"]:<10} {m["modem"]["role"]:<9} '
              f'{m["n_calls"]:>5} {m["n_probes"]:>7} '
              f'{m["n_bins_read"]:>3}/{m["n_bins_total"]:<3} {t:>8.2f}  '
              f'{ev:<11}  {m["measured_utc"][:10]}')
    return 0


def cmd_compare(a):
    ms = [m for m in load_all()
          if not a.modem or m["modem"]["role"] == a.modem]
    if not ms:
        sys.exit("linesweep: no models match. Nothing to compare.")
    hz = sorted({b["hz"] for m in ms for b in m["bins"]})
    print(f'response by frequency, dB relative to each arm\'s own '
          f'{hz[0]:.0f} Hz bin\n')
    head = "  %6s" % "Hz"
    for m in ms:
        head += " %14s" % f'{m["impedance"]["asserted"]}/{m["modem"]["role"]}'
    print(head)
    for f in hz:
        row = "  %6.0f" % f
        for m in ms:
            b = next((x for x in m["bins"] if x["hz"] == f), None)
            row += " %14s" % ("--" if not b or b["db"] is None
                              else f'{b["db"]:+.2f}')
        print(row)
    print()
    for m in ms:
        t = m["tilt"]["db"] if m["tilt"] else float("nan")
        print(f'  {m["impedance"]["asserted"]:<10}/{m["modem"]["role"]:<9} '
              f'tilt {t:+.2f} dB over {TILT_BAND[0]:.0f}-{TILT_BAND[1]:.0f} Hz'
              f'   (n={m["n_probes"]} probes, {m["n_calls"]} calls)')
    print('\n  "--" is a bin with no reading: absent, or floored in more than '
          f'{FLOOR_MAX:.0%} of probes.\n  It is not zero and it is not '
          'interpolated.')
    if a.emit_chanshim:
        print("\n" + "-" * 68)
        for m in ms:
            pts = [(b["hz"], b["db"]) for b in m["bins"] if b["db"] is not None]
            print(f'\n# measured {m["measured_utc"][:10]}, ATA impedance '
                  f'{m["impedance"]["asserted"]}, modem {m["modem"]["role"]},'
                  f' n={m["n_probes"]} probes')
            print(f'"{m["impedance"]["asserted"]}-{m["modem"]["role"]}-probe": (')
            print("    np.array([%s], float)," % ", ".join(f"{f:.0f}" for f, _ in pts))
            print("    np.array([%s], float)),"
                  % ", ".join(f"{d:.2f}" for _, d in pts))
    return 0


def cmd_plot(a):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    ms = load_all()
    if not ms:
        sys.exit("linesweep: no models to plot.")
    roles = sorted({m["modem"]["role"] for m in ms})
    n = 1 + (len(roles) if a.per_modem and len(roles) > 1 else 0)
    fig, axes = plt.subplots(n, 1, figsize=(9, 3.4 * n), squeeze=False)
    axes = axes[:, 0]

    def draw(ax, models, title):
        for m in models:
            f = [b["hz"] for b in m["bins"]]
            d = [b["db"] if b["db"] is not None else np.nan for b in m["bins"]]
            ax.plot(f, d, marker="o", ms=3, lw=1.3,
                    label=f'{m["impedance"]["asserted"]}/{m["modem"]["role"]}'
                          f'  (n={m["n_probes"]})')
        ax.set_title(title, fontsize=10)
        ax.set_xlabel("Hz"); ax.set_ylabel("dB rel. lowest bin")
        ax.grid(alpha=.3); ax.legend(fontsize=7)

    draw(axes[0], ms, "All arms — gaps are bins with no reading, not zeros")
    if n > 1:
        for ax, role in zip(axes[1:], roles):
            draw(ax, [m for m in ms if m["modem"]["role"] == role],
                 f"{role} — impedance comparison")
    fig.tight_layout()
    out = a.out or os.path.join(STORE, "compare.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.savefig(out, dpi=130)
    print(f"wrote {out}")
    print(f"  {len(ms)} arm(s), {len(roles)} modem(s): {', '.join(roles)}")
    return 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--impedance", required=True,
                       help="600r | complex1 | complex2 | ... (YOUR ASSERTION)")
        p.add_argument("--modem", required=True, help="courier | supra | olinet")
        p.add_argument("--ata-config", help="VG204 config dump: records its "
                                            "sha256 and impedance lines")
        p.add_argument("--note")

    p = sub.add_parser("ingest", help="build a model from existing capture logs")
    common(p); p.add_argument("logs", nargs="+")
    p.set_defaults(fn=cmd_ingest)

    p = sub.add_parser("measure", help="place calls, then build a model")
    common(p); p.add_argument("--calls", type=int, default=10)
    p.set_defaults(fn=cmd_measure)

    p = sub.add_parser("list"); p.set_defaults(fn=cmd_list)

    p = sub.add_parser("compare")
    p.add_argument("--modem"); p.add_argument("--emit-chanshim", action="store_true")
    p.set_defaults(fn=cmd_compare)

    p = sub.add_parser("plot")
    p.add_argument("-o", "--out"); p.add_argument("--per-modem", action="store_true",
                                                  default=True)
    p.set_defaults(fn=cmd_plot)

    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
