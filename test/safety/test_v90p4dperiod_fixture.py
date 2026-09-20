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
    require(text.count("measurement calls=2394/2394 transitions=4/4") == 12, text)
    require(text.count("Ed calls=12/192") == 12, text)
    print(f"baseline: exit 0; {count}/{count} checks; 12/12 cycles")
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
    rc, text = run("unknown")
    require(rc == 1 and "unknown observer probe" in text, (rc, text))
    print("unknown probe: exit 1; rejected")
    print("period fixture controls: 8/8 passed (baseline + 6 faults + unknown)")
    if args.artifacts:
        (args.artifacts / "status.json").write_text(json.dumps({
            "binary": str(Path(args.binary).resolve()),
            "sha256": hashlib.sha256(Path(args.binary).read_bytes()).hexdigest(),
            "baseline_checks": count, "controls_passed": 8, "runs": records,
        }, indent=2) + "\n")


if __name__ == "__main__":
    main()
