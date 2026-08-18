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

def build_model(impedance, modem, logs, ata_config=None, note=None,
                binary=None):
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

    # Second band, reported alongside and never instead.  450-3150 is finding
    # 1956's band and stops short of the codec corner, so it is TILT; the wider
    # 300-3400 includes the corner, so it is tilt PLUS band limit.  They are
    # different quantities and quoting one as the other is how a roll-off gets
    # reported as a slope.
    wdb, wn = bandshape.tilt(np.asarray(r["f"], float),
                             np.asarray(r["med"], float),
                             np.asarray(r["ffrac"], float), 300.0, 3400.0)

    # CORNER FREQUENCIES, by linear interpolation BETWEEN TWO MEASURED BINS.
    # That is interpolation, not extrapolation: both endpoints are readings and
    # the answer lies between them.  If the curve never crosses, the corner is
    # None -- it is not pinned to the last bin.
    read = [(b["hz"], b["db"]) for b in bins
            if b["is_reading"] and not b["is_noise_bin"]]
    ref = None
    corners = {}
    if read:
        band = [d for f, d in read if 300.0 <= f <= 1000.0]
        ref = float(np.median(band)) if band else float(read[0][1])
        for drop in (3.0, 6.0):
            hit = None
            for (f0, d0), (f1, d1) in zip(read, read[1:]):
                if f1 < 1000.0:
                    continue
                if (d0 - ref) >= -drop > (d1 - ref):
                    t = ((ref - drop) - d0) / (d1 - d0)
                    hit = round(f0 + t * (f1 - f0), 1)
                    break
            corners[f"minus{int(drop)}db_hz"] = hit

    fit_delta = []
    for hz in (3300.0, 3450.0, 3600.0, 3750.0):
        b = next((x for x in bins if x["hz"] == hz), None)
        if b and b["db"] is not None:
            fit_delta.append({"hz": hz, "measured_db": round(b["db"], 2),
                              "fit_db": round(float(bandshape.fit_db(hz)), 2),
                              "delta_db": round(b["db"]
                                                - float(bandshape.fit_db(hz)), 2)})

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
                 "n_points": tn} if tilt is not None else None,
        "tilt_wide": ({"band_hz": [300.0, 3400.0], "db": round(float(wdb), 3),
                       "n_points": wn} if wdb is not None else None),
        "passband_ref_db": (None if ref is None else round(ref, 3)),
        "corners_hz": corners,
        "n_bins_floored": sum(1 for b in bins if not b["is_reading"]),
        "vs_1907_fit": fit_delta,
        "note": note,
        "binary": binary,
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
    m = build_model(a.impedance, a.modem, a.logs, a.ata_config, a.note,
                    getattr(a, "_binary", os.environ.get("SLMODEMD")))
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


def probe_capable(binary):
    """Does this slmodemd actually emit the V.34 line probe?

    None means "could not tell", which is deliberately not the same as False:
    an unreadable binary is a reason to warn, not a reason to refuse.
    """
    try:
        out = subprocess.run(["strings", "-a", binary], capture_output=True,
                             text=True, timeout=60).stdout
        return "V34PROBEBINS" in out
    except Exception:
        return None


def cmd_measure(a):
    # RESOLVE THE BINARY HERE AND PASS IT DOWN.  row.sh defaults SLMODEMD to
    # $ROOT/slmodemd/slmodemd -- the FORK's blob-based build -- and this tool
    # used to pre-flight a different default (build/hybrid-fit/slmodemd-fit).
    # So it validated one binary and measured with another: ten real calls came
    # back with zero probes and zero V34DATARATE, because the blob does not
    # carry our instrumentation at all.  Two defaults for one thing is the
    # whole bug; there is now one, and it is passed explicitly.
    #
    # `preemph_fit_ab.sh` is the pattern that always worked and is worth
    # copying rather than rediscovering: SLMODEMD=$FIT together with
    # DSPLIB_V34_DUMP_PROBE_BINS=1, both on the same command.
    # The hybrid lives in the MAIN tree, not in whichever worktree this copy of
    # the script is sitting in -- `os.path.dirname(BENCH)` gave a path under
    # the worktree and found nothing.  Candidates in order, and the one chosen
    # is printed, because "which binary" is the question this whole bug was.
    CANDIDATES = [
        "/home/philpem/dev/sip-D-modem/claude_re/build/hybrid-fit/slmodemd-fit",
        os.path.join(os.path.dirname(BENCH), "build", "hybrid-fit",
                     "slmodemd-fit"),
    ]
    sl = os.environ.get("SLMODEMD")
    if not sl:
        sl = next((c for c in CANDIDATES if os.access(c, os.X_OK)), None)
    if not sl or not os.access(sl, os.X_OK):
        sys.exit("linesweep: no usable slmodemd. Tried:\n  " +
                 "\n  ".join(CANDIDATES) +
                 "\nSet SLMODEMD, or build the hybrid "
                 "(tools/hybrid.sh && tools/hybrid_link.sh).")
    cap = probe_capable(sl)
    if cap is False:
        sys.exit(
            f"linesweep: {sl}\n"
            "  does not contain V34PROBEBINS, so it cannot emit the V.34 line\n"
            "  probe and every call would reduce to zero probes. NOT DIALLING.\n\n"
            "  The probe dump is V.34 bench instrumentation: it is in OUR\n"
            "  reconstruction, not in the blob, and master's source no longer\n"
            "  has it either. The fork's slmodemd/slmodemd will never emit it.\n"
            "  Use a hybrid build, or a build from `v34-instrumentation`, or\n"
            "  run `ingest` over archived logs instead.")
    print(f'  binary: {sl}')
    print(f'  pre-flight: {"carries V34PROBEBINS" if cap else "UNVERIFIABLE"}')

    row = os.path.join(BENCH, "row.sh")
    ext = subprocess.run(["bash", "-c",
                          f'. "{BENCH}/modems.sh" && modem_ext {a.modem}'],
                         capture_output=True, text=True).stdout.strip()
    if not ext:
        sys.exit(f"linesweep: cannot resolve an extension for '{a.modem}'")
    stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M")
    labels = []
    # THIS DOES NOT WAIT FOR A QUIET MACHINE, DELIBERATELY.  Whether the box
    # is quiet enough to measure on is the caller's policy, not this tool's: a
    # measurement tool that blocks on its own judgement cannot be used to
    # characterise a loaded machine on purpose, and it hides a decision the
    # operator should be making.  `waitquiet.sh` is right there:
    #
    #     testbench/waitquiet.sh && linesweep.py measure ...
    #
    # Finding 1951 is why it matters -- a call at load 4.28 returned CONNECT
    # 4800 against 31200 quiet.  The load is recorded with every arm so a
    # result taken on a busy box can at least be identified later.
    print(f'measuring {a.modem} (ext {ext}) at impedance '
          f'{a.impedance} -- {a.calls} calls')
    try:
        print(f'  load at start: {os.getloadavg()[0]:.2f}  '
              f'(this tool does not gate on it -- see waitquiet.sh)')
    except OSError:
        pass
    for i in range(1, a.calls + 1):
        lab = f"ls-{a.impedance}-{a.modem}-{stamp}-{i}"
        # THE FULL ENVIRONMENT row.sh NEEDS, copied from cx2batch.sh:59 which
        # is the invocation that has always worked.  Getting this wrong is not
        # a degraded measurement, it is no measurement:
        #
        #   SLMODEMD_IODELAY  V.34 CANNOT CONNECT below 86 and the driver
        #                     answers 0 (deviation D77).  Omit it and the call
        #                     never reaches V.34 -- no probe, no V34DATARATE,
        #                     and no .wav either, because the recordings are
        #                     written from the datapump's /tmp/modem_*.raw and
        #                     there is nothing to write.  That is exactly the
        #                     "10 ok / 0 probes" run this comment exists for.
        #   TTY               which far-end modem row.sh sets up and reads the
        #                     link report from.
        #   HOLD              how long to hold the call, so training and the
        #                     probe have time to happen.
        env = dict(os.environ)
        env["SLMODEMD"] = sl
        env["DSPLIB_V34_DUMP_PROBE_BINS"] = os.environ.get(
            "DSPLIB_V34_DUMP_PROBE_BINS", "1")
        env["SLMODEMD_IODELAY"] = os.environ.get("SLMODEMD_IODELAY", "240")
        env["TTY"] = os.environ.get("TTY", a.modem)
        env["HOLD"] = os.environ.get("HOLD", "45")
        r = subprocess.run(["timeout", "220", row,
                            os.path.join(BENCH, "captures", lab), "pty", ext],
                           capture_output=True, text=True, env=env)

        # ROW.SH EXITING 0 IS NOT A CALL THAT CONNECTED, and reporting its exit
        # code as "ok" is how ten useless calls looked like ten good ones.
        # Score the artefacts instead.
        base = os.path.join(BENCH, "captures", lab)
        conn = probes = 0
        try:
            with open(base + ".call.log", errors="replace") as fh:
                conn = fh.read().count("CONNECT")
        except OSError:
            pass
        try:
            with open(base + ".slmodemd.log", errors="replace") as fh:
                probes = fh.read().count("V34PROBEBINS")
        except OSError:
            pass
        ok = probes > 0
        print(f'  {i}/{a.calls} {lab}  connect={conn} probes={probes}'
              f'  {"OK" if ok else "NO PROBE -- not usable"}')
        if ok:
            labels.append(lab)
    print(f'  {len(labels)} of {a.calls} calls completed')
    if not labels:
        sys.exit("linesweep: no call completed; nothing to model.")
    a.logs = [os.path.join(BENCH, "captures", l + ".slmodemd.log")
              for l in labels]
    a._binary = sl
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



def _fmt_arm(m):
    out = []
    a = out.append
    imp, role = m["impedance"]["asserted"], m["modem"]["role"]
    ev = "EVIDENCED" if m["impedance"]["evidence"] else "ASSERTED (operator's word)"
    a(f'{imp} / {role}')
    a(f'  provenance     {ev}')
    if m["impedance"]["evidence"]:
        e = m["impedance"]["evidence"]
        a(f'                 {os.path.basename(e["path"])}  '
          f'sha256 {e["sha256"][:16]}...')
        a(f'                 {e["ports_setting_impedance"]} port(s) set it: '
          f'{", ".join(e["impedance_lines"]) or "(none in file)"}')
    if m["modem"].get("identity"):
        a(f'  modem          {m["modem"]["identity"][:88]}')
    a(f'  measured       {m["measured_utc"]}')
    a(f'  denominators   {m["n_calls"]} calls, {m["n_probes"]} probes, '
      f'{m["n_bins_read"]}/{m["n_bins_total"]} bins read '
      f'({m["n_bins_floored"]} floored)')
    if m.get("note"):
        a(f'  note           {m["note"]}')
    a('')
    t, w = m.get("tilt"), m.get("tilt_wide")
    if t:
        a(f'  tilt   {t["band_hz"][0]:.0f}-{t["band_hz"][1]:.0f} Hz  '
          f'{t["db"]:+6.2f} dB over {t["n_points"]} bins   '
          f'<- 1956 band, stops short of the corner: this IS the tilt')
    if w:
        a(f'  tilt   {w["band_hz"][0]:.0f}-{w["band_hz"][1]:.0f} Hz  '
          f'{w["db"]:+6.2f} dB over {w["n_points"]} bins   '
          f'<- includes the corner: tilt PLUS band limit')
    c = m.get("corners_hz") or {}
    ref = m.get("passband_ref_db")
    if ref is not None:
        a(f'  passband ref   {ref:+.2f} dB  (median 300-1000 Hz)')
    for k, lbl in (("minus3db_hz", "-3 dB"), ("minus6db_hz", "-6 dB")):
        v = c.get(k)
        a(f'  corner {lbl}   ' + (f'{v:.0f} Hz  (interpolated between two '
                                  f'measured bins)' if v else
                                  'not crossed within the measured band'))
    if m.get("vs_1907_fit"):
        a('')
        a("  against the curve chanshim.py hardcodes (finding 1907's fit):")
        a(f'    {"Hz":>6} {"measured":>10} {"fit":>9} {"delta":>9}')
        for d in m["vs_1907_fit"]:
            a(f'    {d["hz"]:>6.0f} {d["measured_db"]:>10.2f} '
              f'{d["fit_db"]:>9.2f} {d["delta_db"]:>+9.2f}')
    return out


def cmd_stats(a):
    ms = [m for m in load_all() if not a.modem or m["modem"]["role"] == a.modem]
    if not ms:
        sys.exit("linesweep: no models match. Nothing to report.")
    for m in ms:
        print("=" * 74)
        print("\n".join(_fmt_arm(m)))
        print()
    if len(ms) >= 2:
        print("=" * 74)
        print("ARM TO ARM\n")
        base = ms[0]
        for m in ms[1:]:
            print(f'  {m["impedance"]["asserted"]}/{m["modem"]["role"]} '
                  f'minus {base["impedance"]["asserted"]}/{base["modem"]["role"]}')
            for k, lbl in (("tilt", "tilt 450-3150"), ("tilt_wide", "tilt 300-3400")):
                if m.get(k) and base.get(k):
                    print(f'    {lbl:<16} {m[k]["db"] - base[k]["db"]:+.2f} dB')
            for k, lbl in (("minus3db_hz", "-3 dB corner"),
                           ("minus6db_hz", "-6 dB corner")):
                x, y = (m.get("corners_hz") or {}).get(k), (base.get("corners_hz") or {}).get(k)
                if x and y:
                    print(f'    {lbl:<16} {x - y:+.0f} Hz  ({y:.0f} -> {x:.0f})')
            print(f'    n                {base["n_probes"]} vs {m["n_probes"]} probes'
                  f'  -- read every number above against these')
            print()
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
        facts = []
        for m in models:
            f = [b["hz"] for b in m["bins"]]
            d = [b["db"] if b["db"] is not None else np.nan for b in m["bins"]]
            ln, = ax.plot(f, d, marker="o", ms=3, lw=1.4,
                          label=f'{m["impedance"]["asserted"]}/'
                                f'{m["modem"]["role"]}  (n={m["n_probes"]}p/'
                                f'{m["n_calls"]}c)')
            col = ln.get_color()
            # tilt, drawn where it was measured so the number has a place
            t = m.get("tilt")
            if t and m.get("passband_ref_db") is not None:
                lo_, hi_ = t["band_hz"]
                y0 = m["passband_ref_db"]
                ax.plot([lo_, hi_], [y0, y0 + t["db"]], ls=":", lw=1.6,
                        color=col, alpha=.9)
                ax.annotate(f'{t["db"]:+.2f} dB', xy=(hi_, y0 + t["db"]),
                            xytext=(4, -10), textcoords="offset points",
                            fontsize=7, color=col)
            for k, ls_ in (("minus3db_hz", "--"), ("minus6db_hz", "-.")):
                hz = (m.get("corners_hz") or {}).get(k)
                if hz:
                    ax.axvline(hz, ls=ls_, lw=.9, color=col, alpha=.55)
                    ax.annotate(f'{k[6]}{"" if k[5]=="3" else ""}'
                                f'{"-3dB" if "3" in k else "-6dB"} {hz:.0f}',
                                xy=(hz, ax.get_ylim()[0]), xytext=(2, 4),
                                textcoords="offset points", fontsize=6,
                                color=col, rotation=90)
            c = m.get("corners_hz") or {}
            facts.append(
                f'{m["impedance"]["asserted"]}/{m["modem"]["role"]}: '
                f'tilt {t["db"]:+.2f} dB (450-3150), '
                f'-3dB {c.get("minus3db_hz") or float("nan"):.0f} Hz, '
                f'-6dB {c.get("minus6db_hz") or float("nan"):.0f} Hz, '
                f'{m["n_bins_read"]}/{m["n_bins_total"]} bins'
                if t else f'{m["impedance"]["asserted"]}: tilt not computable')
        # the curve the emulator hardcodes, for reference only
        fr = np.linspace(150, 3750, 200)
        ax.plot(fr, [bandshape.fit_db(x) for x in fr], color="0.55", lw=1.0,
                ls=(0, (6, 3)), label="chanshim fit (finding 1907)")
        ax.set_title(title, fontsize=10)
        ax.set_xlabel("Hz"); ax.set_ylabel("dB rel. passband")
        ax.grid(alpha=.3); ax.legend(fontsize=7, loc="lower left")
        if facts:
            ax.text(0.015, 0.04, "\n".join(facts), transform=ax.transAxes,
                    fontsize=6.5, va="bottom", family="monospace",
                    bbox=dict(fc="white", ec="0.7", alpha=.85, pad=3))

    draw(axes[0], ms, "All arms  —  gaps are bins with no reading, not zeros; "
                      "dotted = measured tilt, dashed grey = chanshim's fit")
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

    p = sub.add_parser("stats", help="full text report, all the numbers")
    p.add_argument("--modem"); p.set_defaults(fn=cmd_stats)

    p = sub.add_parser("plot")
    p.add_argument("-o", "--out"); p.add_argument("--per-modem", action="store_true",
                                                  default=True)
    p.set_defaults(fn=cmd_plot)

    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
