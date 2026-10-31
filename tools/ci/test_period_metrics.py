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

    def run_cli(self, reviews=None, base_sha=BASE, digest=DIGEST,
                displaced=None):
        for name, value in (("base", self.base), ("head", self.head)):
            (self.root / (name + ".json")).write_text(json.dumps(value))
        args = ["period_metrics.py", "--base", str(self.root / "base.json"),
                "--head", str(self.root / "head.json"), "--check"]
        if reviews is not None:
            (self.root / "reviews.json").write_text(json.dumps(dict(version=1, reviews=reviews)))
            args += ["--reviewed-losses", str(self.root / "reviews.json"),
                     "--base-sha", base_sha, "--head-objects", str(self.root)]
        if displaced is not None:
            (self.root / "displaced.json").write_text(
                json.dumps(dict(version=1, displaced=displaced)))
            args += ["--displaced", str(self.root / "displaced.json"),
                     "--head-objects", str(self.root)]
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
        self.assertIn("| EXACT functions | 2 | 1 | -1 | **:red_circle: regression**", text)
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

    def displacement(self, metric="exact", symbol="lost", body=DIGEST,
                     selected="a" * 64,
                     evidence="docs/method/refinement.md"):
        entry = {"symbol": symbol, "metric": metric, "from": "EXACT",
                 "to": "REGALLOC", "phase": "order", "cause": "reviewed",
                 "evidence": evidence,
                 "recovery": "re-measure after the cause settles"}
        if metric in ("exact", "both"):
            entry["body_sha256"] = body
        if metric in ("selected", "both"):
            entry["selected_body_sha256"] = selected
        return entry

    def test_matching_displacement_passes_and_is_labelled(self):
        status, text = self.run_cli(displaced=[self.displacement()])
        self.assertEqual(status, 0)
        self.assertIn("0 reviewed, 1 displaced, 0 unreviewed / 1 lost", text)
        self.assertIn("Reviewed displacement `lost`", text)
        self.assertNotIn("Reviewed loss `lost`", text)

    def test_changed_displacement_body_fails(self):
        changed = m.body_digest(RAW[:-1] + b"\x90", RELOCS)
        status, text = self.run_cli(
            displaced=[self.displacement(body=changed)], digest=DIGEST)
        self.assertEqual(status, 1)
        self.assertIn("displacement body hash does not match head", text)

    def test_selected_only_displacement_does_not_excuse_exact_loss(self):
        status, text = self.run_cli(
            displaced=[self.displacement(metric="selected")])
        self.assertEqual(status, 1)
        self.assertIn("selected-only displacement does not excuse an EXACT "
                      "loss", text)

    def test_unreviewed_loss_still_fails_with_a_ledger(self):
        status, text = self.run_cli(
            displaced=[self.displacement(symbol="other")])
        self.assertEqual(status, 1)
        self.assertIn("Unreviewed loss `lost`: no reviewed displacement", text)

    def test_review_and_displacement_are_separate_labels(self):
        status, text = self.run_cli([review()],
                                    displaced=[self.displacement(symbol="x")])
        self.assertEqual(status, 0)
        self.assertIn("1 reviewed, 0 displaced, 0 unreviewed / 1 lost", text)
        self.assertIn("Reviewed loss `lost`", text)


class FidelityReportControls(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / "metrics.json"
        sys.path.insert(0, str(m.ROOT / "tools/toolchain"))
        import fidelity
        self.fidelity = fidelity

    def sample(self, unavailable=False):
        data = m.metrics(0, [], 0, 0, 0, 1, 0, 0, 1, 0)
        row = dict(fidelity="NONMATCH", reference_bytes=12, candidate_bytes=12,
                   byte_delta=0, instructions=None)
        if unavailable:
            row["run_unavailable"] = "empty instruction stream"
        else:
            row["instructions"] = dict(reference_instructions=4, candidate_instructions=4,
                                       longest_run=2, reference_percent=50.0)
        data.update(nonmatch=1, function_fidelity={"example": row})
        data["instruction_runs"] = self.fidelity.summary(data["function_fidelity"])
        return data

    def load(self, data):
        self.path.write_text(json.dumps(data))
        return m.load(self.path)

    def test_new_report_loads_and_prints_counts_percentages(self):
        base, head = self.sample(), self.sample()
        head["function_fidelity"]["example"]["instructions"].update(longest_run=3, reference_percent=75.0)
        head["instruction_runs"] = self.fidelity.summary(head["function_fidelity"])
        self.assertEqual(self.load(head), head)
        text = "\n".join(m.render(base, head)[0])
        self.assertIn("2 / 4 (50.0%)", text)
        self.assertIn("3 / 4 (75.0%)", text)
        self.assertIn("1 changed / 1 common functions", text)

    def test_legacy_counts_remain_readable_and_unknown_runs_are_not_zero(self):
        data = m.metrics(0, [], 0, 0, 0, 1, 0, 0, 1, 0)
        self.assertEqual(self.load(data), data)
        text = "\n".join(m.render(data, self.sample())[0])
        self.assertIn("Base: unavailable (legacy report)", text)
        self.assertIn("Per-function run changes unavailable", text)

    def test_invalid_new_reports_are_refused(self):
        import copy
        valid = self.sample()
        mutations = [
            ("wrong nonmatch", lambda x: x.update(nonmatch=0)),
            ("missing function", lambda x: x["function_fidelity"].clear()),
            ("missing summary", lambda x: x.pop("instruction_runs")),
            ("bad summary", lambda x: x["instruction_runs"].update(measured_functions=0)),
            ("unknown normalization", lambda x: x["instruction_runs"].update(normalization="unknown")),
            ("false match", lambda x: x["function_fidelity"]["example"].update(fidelity="EXACT")),
            ("negative bytes", lambda x: x["function_fidelity"]["example"].update(candidate_bytes=-1)),
            ("wrong byte delta", lambda x: x["function_fidelity"]["example"].update(byte_delta=1)),
            ("longer than reference", lambda x: x["function_fidelity"]["example"]["instructions"].update(longest_run=5)),
            ("nonfinite percentage", lambda x: x["function_fidelity"]["example"]["instructions"].update(reference_percent=float("nan"))),
            ("wrong percentage", lambda x: x["function_fidelity"]["example"]["instructions"].update(reference_percent=75.0)),
            ("empty reference", lambda x: x["function_fidelity"]["example"]["instructions"].update(reference_instructions=0)),
            ("unexplained unavailable", lambda x: x["function_fidelity"]["example"].update(instructions=None)),
        ]
        for name, change in mutations:
            with self.subTest(corruption=name):
                bad = copy.deepcopy(valid)
                change(bad)
                with self.assertRaises(ValueError):
                    self.load(bad)
        self.assertEqual(self.load(valid), valid)

    def test_unavailable_is_reported_outside_measured_denominator(self):
        data = self.sample(unavailable=True)
        self.assertEqual(self.load(data), data)
        text = "\n".join(m.render(data, data)[0])
        self.assertIn("measured **0 / 1 functions**, 1 unavailable", text)
        self.assertNotIn("(0.0%)", text[text.index("### Contiguous"):])

    def test_no_changes_is_an_explicit_measured_result(self):
        data = self.sample()
        text = "\n".join(m.render(data, data)[0])
        self.assertIn("0 changed / 1 common functions", text)

    def test_size_bucket_transfer_does_not_report_fidelity_progress(self):
        base = m.metrics(0, [], 0, 0, 0, 1, 0, 0, 1, 0)
        head = m.metrics(0, [], 0, 0, 0, 0, 1, 0, 1, 0)
        text = "\n".join(m.render(base, head)[0])
        self.assertIn("| NONMATCH functions | 1 | 1 | +0 | :white_circle: no change", text)
        self.assertNotIn("| Different size |", text)
        self.assertNotIn("| Different bytes, same size |", text)

    def test_nonmatch_to_regalloc_is_progress_not_red_regalloc(self):
        base = m.metrics(0, [], 0, 0, 0, 1, 0, 0, 1, 0)
        head = m.metrics(0, [], 1, 0, 0, 0, 0, 0, 1, 0)
        text = "\n".join(m.render(base, head)[0])
        self.assertIn("| REGALLOC functions | 0 | 1 | +1 | :white_circle: informational", text)
        self.assertIn("| EXACT + REGALLOC functions | 0 | 1 | +1 | :green_circle: better", text)

    def test_unresolved_is_not_credited_as_matched(self):
        data = m.metrics(0, [], 0, 0, 1, 0, 0, 0, 1, 0)
        data["grade01"] = 1  # Legacy accounting must not leak into the headline.
        text = "\n".join(m.render(data, data)[0])
        self.assertIn("| EXACT + REGALLOC functions | 0 | 0 |", text)
        self.assertIn("| UNRESOLVED functions (not certified) | 1 | 1 |", text)


    def test_per_function_exact_names_must_match_protected_names(self):
        data = self.sample()
        data.update(exact=1, exact_symbols=["different-symbol"], bytes=0, nonmatch=0)
        data["function_fidelity"]["example"]["fidelity"] = "EXACT"
        with self.assertRaisesRegex(ValueError, "EXACT membership"):
            self.load(data)

    def test_legacy_protected_membership_cannot_be_reassigned(self):
        data = self.sample()
        data["grade01_symbols"] = ["example"]
        with self.assertRaisesRegex(ValueError, "protected membership"):
            self.load(data)


if __name__ == "__main__":
    unittest.main()
