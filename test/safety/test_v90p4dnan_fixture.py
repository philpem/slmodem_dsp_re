"""Observer controls on the actual modern binary; NOT an exemption validator.

Run with make J=1 -j1 safety-p4dnan-gcc14. A red baseline is never a
mutation catch: require the exact baseline plus named additional diagnostics.
This control is bounded to the measured GCC 14 ILP32 fixture profile.
"""
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
BINARY = Path(os.environ.get("DSPLIB_P4DNAN_BINARY",
                             str(ROOT / "build/test/t_v90p4dnan")))


def run(fault=None, evidence=False):
    env = dict(os.environ, DSPLIB_MAX_REPORT="0")
    env.pop("DSPLIB_P4DNAN_FAULT", None)
    env.pop("DSPLIB_TRANSCRIPT_EVIDENCE", None)
    if fault is not None:
        env["DSPLIB_P4DNAN_FAULT"] = fault
    if evidence:
        env["DSPLIB_TRANSCRIPT_EVIDENCE"] = "1"
    return subprocess.run([str(BINARY)], env=env, capture_output=True,
                          text=True, timeout=30)


class FixtureControls(unittest.TestCase):
    def check_run(self, fault, failures, total=344):
        r = run(fault)
        self.assertEqual(r.returncode, 1, r.stderr)
        self.assertEqual(r.stdout, "FAIL V90Phase4Demodulator: strict keep-rate "
                         "sentinel %d/%d checks failed\n" % (failures, total))
        self.assertEqual(sum(line.startswith("test/unit/t_v90p4dnan.cpp:")
                             for line in r.stderr.splitlines()), failures, r.stderr)
        return r.stderr

    def test_baseline(self):
        err = self.check_run(None, 4)
        for trial in (800000, 800100):
            self.assertIn("keep-rate [input %d]  got 0, reference 1" % trial, err)
            self.assertIn("p4d.silence [input %d]  got 0, reference 1" % trial, err)

    def test_same_failing_trial_observers(self):
        # Fault -> total failing assertions and an independently named observer.
        probes = {
            "return": (6, "return expected [input 1600000]"),
            "progress": (6, "progress expected [input 1600000]"),
            "state": (6, "state expected [input 1600000]"),
            "unrelated": (5, "remaining state"),
            "guard": (6, "p4d guard"),
            "peer": (6, "cursor expected [input 1600000]"),
            "parameter": (5, "params immutable"),
        }
        passed = 0
        for fault, (failures, observer) in probes.items():
            with self.subTest(fault=fault):
                err = self.check_run(fault, failures)
                self.assertIn(observer, err)
                self.assertIn("keep-rate [input 800000]  got 0, reference 1", err)
                self.assertIn("keep-rate [input 800100]  got 0, reference 1", err)
                passed += 1
        print("same-failing-trial additional-failure controls: %d/7 fired" % passed)

    def test_transcript_same_failed_assertion(self):
        # Both are red at precisely the same assertion/input. Compare underlying
        # bytes, NOT the identical Boolean diagnostic. No exemption is granted.
        baseline, corrupt = run(evidence=True), run("transcript", evidence=True)
        for r in (baseline, corrupt):
            self.assertEqual(r.returncode, 1)
            self.assertEqual(r.stdout, "FAIL V90Phase4Demodulator: strict keep-rate "
                             "sentinel 4/344 checks failed\n")
        records = []
        for r in (baseline, corrupt):
            rows = [line.split() for line in r.stderr.splitlines()
                    if line.startswith("TRANSCRIPT1 ")]
            self.assertEqual(len(rows), 12)
            self.assertEqual([int(row[1]) for row in rows], list(range(1, 13)))
            self.assertEqual(len({row[3] for row in rows}), 12)
            for row in rows:
                self.assertEqual(row[4], "1")
                self.assertEqual(len(bytes.fromhex(row[8])), int(row[7]))
                self.assertEqual(len(bytes.fromhex(row[11])), int(row[10]))
            records.append(rows)
        a, b = records[0][0], records[1][0]
        self.assertEqual(a[3], "800000")
        self.assertEqual(b[3], "800000")
        self.assertEqual(bytes.fromhex(b[8]), bytes.fromhex(a[8]) + b"injected diagnostic\r\n")
        self.assertEqual(int(b[6]), int(a[6]) + 1)
        self.assertEqual(a[9:], b[9:])
        self.assertEqual(records[0][1:], records[1][1:])
        print("same-failed-transcript raw-evidence control: 1/1 fired (12 records/run)")

    def test_constant_flags(self):
        passed = 0
        for fault, failures in (("always0", 12), ("always1", 14)):
            with self.subTest(fault=fault):
                err = self.check_run(fault, failures)
                self.assertIn("finite/zero flag expected", err)
                passed += 1
        print("constant-flag negative controls: %d/2 fired" % passed)

    def test_unknown_probe(self):
        self.assertIn("unknown fault probe [input 800000]",
                      self.check_run("unknown", 5, 345))


if __name__ == "__main__":
    unittest.main(verbosity=2)
