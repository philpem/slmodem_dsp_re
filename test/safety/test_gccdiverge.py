"""Fail-closed migration controls, run with python3 -m unittest discover."""
import importlib.util
import pathlib
import unittest

spec = importlib.util.spec_from_file_location(
    "gccdiverge", pathlib.Path(__file__).resolve().parents[2] / "tools/gccdiverge.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class TranscriptControls(unittest.TestCase):
    out = "FAIL expected group 1/2 checks failed\n"
    err = "test/unit/t_probe.c:42: expected assertion [input 7]  got 1, reference 2\n"

    def check(self, code=1, out=None, err=None, expected=False, contract=None):
        out = self.out if out is None else out
        err = self.err if err is None else err
        entry = {"exact_transcript_v1": contract or {"stdout": self.out, "stderr": self.err}}
        self.assertEqual(module.literal_output_matches(entry, code, out, err)[0], expected)

    def test_positive(self):
        self.check(expected=True)

    def test_sigsegv(self):
        self.check(code=-11)

    def test_abnormal_exit(self):
        self.check(code=2)

    def test_empty(self):
        self.check(out="", err="")

    def test_unexpected_assertion_same_group(self):
        self.check(err=self.err.replace("expected assertion", "guard corrupted"))

    def test_unexpected_input(self):
        self.check(err=self.err.replace("input 7", "input 8"))

    def test_missing_summary(self):
        self.check(out="")

    def test_truncated_summary(self):
        self.check(out=self.out[:-8])

    def test_now_green(self):
        self.check(code=0)

    def test_legacy_blocked(self):
        self.assertFalse(module.declared_failure({"checks": ["expected group"]},
                                                1, self.out, self.err)[0])

    def test_lossy_same_tag_transcript_never_authorized(self):
        # Both underlying mismatches serialize as the same Boolean assertion.
        # Syntax/equality matching is intentionally NOT an exemption mechanism.
        reference = "coefs sum = +0.005849"
        for actual in ["coefs sum = +0.005850", "totally incorrect diagnostic"]:
            err = "test/unit/t_probe.c:42: transcript [input 7]  got %d, reference 1\n" % (
                actual == reference)
            entry = {"exact_transcript_v1": {"stdout": self.out, "stderr": err}}
            self.assertTrue(module.literal_output_matches(entry, 1, self.out, err)[0])
            self.assertFalse(module.declared_failure(entry, 1, self.out, err)[0])

    def test_invalid_reviewed_contracts(self):
        for out, err in [("FAIL expected group 1/0 checks failed\n", self.err),
                         ("FAIL expected group 2/3 checks failed\n", self.err),
                         ("malformed\n", self.err), ("", "")]:
            with self.subTest(out=out):
                self.check(out=out, err=err, contract={"stdout": out, "stderr": err})


if __name__ == "__main__":
    unittest.main(verbosity=2)
