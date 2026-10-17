#!/usr/bin/env python3
"""Firing and stale-evidence controls for reviewed codegen losses; no C build."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import period_metrics as m


BASE = "1" * 40
RAW = bytes.fromhex("b800000000c3")
RELOCS = {1: ("R_386_32", ("symbol", "owner", 0))}
DIGEST = m.body_digest(RAW, RELOCS)


def review():
    return dict(base_sha=BASE, symbol="lost", head_body_sha256=DIGEST,
                accepted_commit="2" * 40,
                evidence_commit="3" * 40,
                evidence="docs/method/refinement.md#review-exact-losses-against-source-fidelity")


class ReviewedLossControls(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.base = m.metrics(2, ["keep", "lost"], 1, 0, 0, 0, 0, 0, 3, 100)
        self.head = m.metrics(2, ["keep", "gain"], 1, 0, 0, 0, 0, 0, 3, 90)

    def run_cli(self, reviews=None, base_sha=BASE, digest=DIGEST):
        for name, value in (("base", self.base), ("head", self.head)):
            (self.root / (name + ".json")).write_text(json.dumps(value))
        args = ["period_metrics.py", "--base", str(self.root / "base.json"),
                "--head", str(self.root / "head.json"), "--check"]
        if reviews is not None:
            (self.root / "reviews.json").write_text(json.dumps(dict(version=1, reviews=reviews)))
            args += ["--reviewed-losses", str(self.root / "reviews.json"),
                     "--base-sha", base_sha, "--head-objects", str(self.root)]
        out = io.StringIO()
        with (patch.object(sys, "argv", args),
              patch.object(m, "function_digest", return_value=digest),
              contextlib.redirect_stdout(out)):
            status = m.main()
        return status, out.getvalue()

    def test_unreviewed_loss_fails_even_with_equal_gain(self):
        status, text = self.run_cli()
        self.assertEqual(status, 1)
        self.assertIn("**Lost byte-identical functions:** `lost`", text)
        self.assertIn("**New byte-identical functions:** `gain`", text)
        self.assertIn("0 reviewed, 1 unreviewed / 1 lost", text)

    def test_exact_review_passes_but_keeps_regression_visible(self):
        status, text = self.run_cli([review()])
        self.assertEqual(status, 0)
        self.assertIn("**:red_circle: regression**", text)
        self.assertIn("1 reviewed, 0 unreviewed / 1 lost", text)
        self.assertIn("Reviewed loss `lost`", text)
        self.assertIn("[evidence]", text)
        self.assertIn("https://github.com/philpem/slmodem_dsp_re/blob/" +
                      "3" * 40 + "/docs/method/refinement.md#", text)
        self.assertIn("-10 bytes", text)

    def test_additional_loss_is_not_covered(self):
        self.head = m.metrics(1, ["gain"], 2, 0, 0, 0, 0, 0, 3, 50)
        status, text = self.run_cli([review()])
        self.assertEqual(status, 1)
        self.assertIn("1 reviewed, 1 unreviewed / 2 lost", text)
        self.assertIn("Unreviewed loss `keep`: no review", text)

    def test_review_does_not_hide_lower_exact_count(self):
        self.head = m.metrics(1, ["keep"], 2, 0, 0, 0, 0, 0, 3, 50)
        status, text = self.run_cli([review()])
        self.assertEqual(status, 0)
        self.assertIn("| Byte-identical functions | 2 | 1 | -1 | **:red_circle: regression**", text)
        self.assertIn("1 reviewed, 0 unreviewed / 1 lost", text)

    def test_stale_base_fails(self):
        status, text = self.run_cli([review()], base_sha="3" * 40)
        self.assertEqual(status, 1)
        self.assertIn("stale base review", text)

    def test_changed_body_fails(self):
        changed = m.body_digest(RAW[:-1] + b"\x90", RELOCS)
        self.assertNotEqual(changed, DIGEST)
        status, text = self.run_cli([review()], digest=changed)
        self.assertEqual(status, 1)
        self.assertIn("changed function bytes or relocation identity", text)

    def test_changed_relocation_target_fails(self):
        changed = m.body_digest(RAW, {1: ("R_386_32", ("symbol", "other", 0))})
        self.assertNotEqual(changed, DIGEST)
        self.assertEqual(self.run_cli([review()], digest=changed)[0], 1)

    def test_changed_relocation_kind_is_distinct(self):
        self.assertNotEqual(DIGEST, m.body_digest(RAW, {1: ("R_386_PC32", RELOCS[1][1])}))

    def test_changed_relocation_offset_is_distinct(self):
        self.assertNotEqual(DIGEST, m.body_digest(RAW, {2: RELOCS[1]}))

    def test_missing_definition_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "0 defining objects"):
            m.function_digest(self.root, "lost")

    def test_duplicate_definitions_are_rejected(self):
        for name in ("a.o", "b.o"):
            (self.root / name).write_bytes(b"fixture")
        fake = SimpleNamespace(sizes=lambda path: {"lost": 6})
        with (patch.dict(sys.modules, byteident=fake),
              self.assertRaisesRegex(ValueError, "2 defining objects")):
            m.function_digest(self.root, "lost")

    def test_absent_or_ambiguous_body_stays_unreviewed(self):
        def missing(symbol):
            raise ValueError("no uniquely defined measured body")
        accepted, rejected = m.review_losses(["lost"], [review()], BASE, missing)
        self.assertEqual(accepted, {})
        self.assertIn("no uniquely defined", rejected["lost"])

    def test_no_loss_does_not_activate_old_review(self):
        self.head = self.base.copy()
        status, text = self.run_cli([review()], base_sha="3" * 40)
        self.assertEqual(status, 0)
        self.assertIn("0 reviewed, 0 unreviewed / 0 lost", text)

    def test_duplicate_review_is_invalid(self):
        path = self.root / "review.json"
        path.write_text(json.dumps(dict(version=1, reviews=[review(), review()])))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            m.load_reviews(path)

    def test_evidence_must_exist(self):
        item = review()
        item["evidence"] = "missing-review-evidence.md"
        path = self.root / "review.json"
        path.write_text(json.dumps(dict(version=1, reviews=[item])))
        with self.assertRaisesRegex(ValueError, "evidence must exist"):
            m.load_reviews(path)

    def test_empty_body_cannot_be_reviewed(self):
        with self.assertRaisesRegex(ValueError, "no measured body"):
            m.body_digest(b"", {})

    def test_evidence_requires_full_revision(self):
        item = review()
        item["evidence_commit"] = "short"
        path = self.root / "review.json"
        path.write_text(json.dumps(dict(version=1, reviews=[item])))
        with self.assertRaisesRegex(ValueError, "full evidence_commit"):
            m.load_reviews(path)


if __name__ == "__main__":
    unittest.main()
