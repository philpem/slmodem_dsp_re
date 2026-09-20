#!/usr/bin/env python3
"""Run the period fixture's bounded observer controls; does not build anything."""
import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path


def require(condition, evidence):
    # Acceptance checks must run even with PYTHONOPTIMIZE or python -O.
    if not condition:
        raise RuntimeError("period observer control failed: %r" % (evidence,))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", help="already built Gentoo period executable")
    parser.add_argument("--artifacts", type=Path, help="save complete per-probe output and status/identity records")
    args = parser.parse_args()
    if args.artifacts:
        args.artifacts.mkdir(exist_ok=True)
    records = []
    env = dict(os.environ)
    env.pop("DSPLIB_P4D_PERIOD_TRACE", None)
    env.pop("DSPLIB_P4D_PERIOD_FAULT", None)

    def run(probe=None):
        trial_env = dict(env)
        if probe:
            trial_env["DSPLIB_P4D_PERIOD_FAULT"] = probe
        result = subprocess.run([args.binary], env=trial_env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=30)
        records.append({"probe": probe, "returncode": result.returncode})
        if args.artifacts:
            (args.artifacts / ((probe or "baseline") + ".log")).write_text(result.stdout)
        return result.returncode, result.stdout

    rc, text = run()
    match = re.search(r"PASS V90Phase4Demodulator: default-timing component silence lifecycle (\d+) checks", text)
    require(rc == 0 and match, (rc, text))
    count = int(match.group(1))
    require(count > 0, count)
    require(text.count("measurement calls=2394/2394 transitions=4/4") == 36, text)
    require(text.count("period fixture: session=0 Ed calls=12/192") == 6, text)
    require(text.count("producer fixture: session=0 Ed calls=12/192") == 24, text)
    require(text.count("session=1 Ed calls=48/192") == 6, text)
    for control in ("corrupt", "missing"):
        require(text.count(f"CP control={control} calls=192/192 accepted=0/2 no Ed entry") == 1, text)
    matrix = re.findall(r"^CP matrix: local=([01]) peer=([01]) case=(\S+)", text, re.M)
    names = {"pair", "CP-only", "CPnot-only", "reverse", "bad-CP",
             "bad-CPnot", "omit-CP", "omit-CPnot", "both-bad", "neither"}
    require(len(matrix) == 40 and set(matrix) == {
        (local, peer, name) for local in "01" for peer in "01" for name in names
    }, matrix)
    require(text.count("CP matrix summary: 40/40 cells, 2856 sample calls per side, "
                       "20 nonzero peer-copy observations") == 1, text)
    for mode, decision in ((1, 988), (2, 622), (3, 0)):
        require(text.count(f"producer boundary: mode={mode} samples=32280 design=1 bits=23 decision1000={decision}") == 1, text)
    require(text.count("producer summary: 2 finite channel cases, 1 missing-calibration fault; evaluator request supplied") == 1, text)
    for mode, decision in ((1, 988), (2, 622), (3, 0)):
        for side in (0, 1):
            require(text.count(f"P3 ctor replay: mode={mode} side={side} calls=12000/32280 state=20 count=0 event=21") == 1, text)
            live = 690 if mode == 3 else 12
            require(text.count(f"P3 study boundary: mode={mode} side={side} calls=50496/70000 state=16 count=0 event=17 DIL=29160 study=29160 cells={live}/768 seen=690/768 Jd=1") == 1, text)
            positive = 0 if mode == 3 else 48
            maximum = 80 if mode == 3 else 116
            require(text.count(f"P3 study design: mode={mode} side={side} result=1 bits=23 selected=48/48 positive={positive}/48 masks=702/768 max=" +
                               ",".join([str(maximum)] * 6) + f" decision1000={decision} manual-map-equal=1 manual-levels-equal=0") == 1, text)
    require(text.count("P3 study path:") == 48, text)
    require(text.count("P3 study summary: 3/3 cases; 50496 calls/side/case; timed study, evaluator request supplied") == 1, text)
    require(text.count("evaluator boundary: no request at silence boundary (noise below rate-down threshold), "
                       "0 gate calls, 1000 blocks; real equalizer/designer/evaluator chain exercised") == 1, text)
    print(f"baseline: exit 0; {count}/{count} checks; 36/36 cycles")
    print("P3: 3/3 ctor-replay counterexamples; 3/3 timed-study cases, 29160 DIL calls/side/case")
    print("producer: 2/2 finite channels, 1/1 missing-calibration control; 32280 DIL samples/side/case")
    print("CP matrix: 40/40 named cells (34 distinct inputs including local request); "
          "2856 calls per side; 20 nonzero peer copies")
    probes = {
        "return": "measurement return agrees",
        "energy": "after energy exact",
        "state": "complete P4D step including pointers and guard",
        "guard": "p4 guard",
        "peer": "ad immutable during measurement",
        "transcript": "measurement transcript exact",
    }
    for probe, assertion in probes.items():
        rc, text = run(probe)
        require(rc == 1 and assertion in text and "FAIL" in text, (probe, rc, text))
        # These mutate an observed result, never the assertion inventory.
        verdict = re.search(r"FAIL V90Phase4Demodulator: default-timing component silence lifecycle (\d+)/(\d+) checks failed", text)
        require(verdict and int(verdict.group(1)) > 0 and int(verdict.group(2)) == count, (probe, text))
        print(f"probe {probe}: exit 1; named observer fired; {verdict.group(1)}/{count} failed")
    # Alter the message, not an observed postimage: CRC rejection must make
    # the positive-entry requirement fail, even when both implementations agree.
    rc, text = run("cp-crc")
    require(rc == 1 and "two CRC-validated CP messages" in text and
            "Ed reached silence within bound" in text and "FAIL" in text, (rc, text))
    print("probe cp-crc: exit 1; corrupted message rejected by positive-entry oracle")
    rc, text = run("calibration-missing")
    require(rc == 1 and "producer usable descending levels" in text and
            "missing calibration changes downstream decision" in text and "FAIL" in text, (rc, text))
    print("probe calibration-missing: exit 1; both sides' missing production rejected by positive oracle")
    rc, text = run("study-missing")
    require(rc == 1 and "P3 usable descending levels required" in text and
            "P3 downstream decision requires study input" in text and "FAIL" in text, (rc, text))
    for side in (0, 1):
        require(f"P3 study design: mode=1 side={side} result=1 bits=23 selected=48/48 positive=0/48" in text, text)
    print("probe study-missing: exit 1; both sides complete study but fail the positive-output oracle")
    for code in (128, 255):
        rc, text = run(f"mapping-{code}")
        verdict = re.search(r"FAIL V90Phase4Demodulator: default-timing component silence lifecycle (\d+)/(\d+) checks failed", text)
        require(rc == 1 and verdict and int(verdict.group(1)) > 0 and
                "mapping code below 128 before use" in text, (code, rc, text))
        for side in (0, 1):
            require(f"mapping range rejection: side={side} phase=5 index=0 code={code} " in text, text)
            require(f"mapping downstream blocked: mode=1 side={side} dm_reset=0 hardDecision=0 P4D=0" in text, text)
        require(text.count("measurement calls=2394/2394 transitions=4/4") == 30, text)
        print(f"probe mapping-{code}: exit 1; named range check fired on both sides; "
              f"downstream blocked; {verdict.group(1)}/{verdict.group(2)} failed")
    rc, text = run("eval-missing")
    require(rc == 1 and "eval design noise positive" in text and "FAIL" in text, (rc, text))
    print("probe eval-missing: exit 1; withheld equalizer history fails the positive design-input oracle")
    rc, text = run("unknown")
    require(rc == 1 and "unknown observer probe" in text, (rc, text))
    print("unknown probe: exit 1; rejected")
    print("period fixture controls: 14/14 passed (baseline with rejection cases + 12 faults + unknown)")
    if args.artifacts:
        (args.artifacts / "status.json").write_text(json.dumps({
            "binary": str(Path(args.binary).resolve()),
            "sha256": hashlib.sha256(Path(args.binary).read_bytes()).hexdigest(),
            "baseline_checks": count, "controls_passed": 14, "runs": records,
        }, indent=2) + "\n")


if __name__ == "__main__":
    main()
