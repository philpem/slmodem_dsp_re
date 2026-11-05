#!/usr/bin/env python3
"""Synthetic detector controls; these do not establish compiler reachability."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
# tools/dis.py must not shadow the standard-library dis imported by unittest.
_TOOLS = str(Path(__file__).resolve().parent)
sys.path[:] = [entry for entry in sys.path if str(Path(entry or '.').resolve()) != _TOOLS]
import unittest
from unittest.mock import patch

sys.path.insert(0, _TOOLS)
import gcc3_search_model as model


def event(name, eligible, before, selected, after, order=None):
    return {"function": name, "constraint": "r", "mode": "SImode", "from": 0, "to": 1,
            "insn_uid": 123, "allocation_order": list(range(53)) if order is None else order,
            "excluded": [], "cursor_before": before, "selected": selected, "cursor_after": after,
            "eligibility": {"class": 12, "class_members": eligible, "live": [],
                            "fixed": [0] * 53, "call_used": [1] * 53,
                            "ever_live": [0] * 53, "reload_completed": 1,
                            "frame_pointer_needed": 0}}


def envelope(events):
    return {"events": events, "errors": [], "exit_codes": [0], "pending": 0}


def baseline():
    # Baseline and held-out values below are deliberately hand-derived.
    return envelope([event("A", [0, 4], 0, 0, 1), event("B", [1, 3], 1, 1, 2),
                     event("C", [2, 5], 2, 2, 3)])


def held_out():
    rows = [event("B", [1, 3], 0, 1, 2), event("A", [0, 4], 2, 4, 5),
            event("C", [2, 5], 5, 5, 6)]
    for index, row in enumerate(rows):
        row["insn_uid"] = 900 + index
    return envelope(rows)


class ScratchModelTest(unittest.TestCase):
    def setUp(self):
        self.trace = baseline()
        self.model = model.calibrate(self.trace)

    def test_calibration_denominator_all_starts_and_raw_events(self):
        self.assertEqual(self.model["counts"], {"observed_events": 3, "modeled_events": 3,
                                              "outside_model": 0, "blocks": 3, "starts_per_block": 53})
        self.assertEqual(self.model["baseline_order"], ["A#1", "B#1", "C#1"])
        self.assertEqual(self.model["blocks"][0]["events"], self.trace["events"][:1])
        for block in self.model["blocks"]:
            self.assertEqual([entry["start"] for entry in block["transitions"]], list(range(53)))
        self.trace["events"][0]["selected"] = 44
        self.assertEqual(self.model["blocks"][0]["events"][0]["selected"], 0)

    def test_json_roundtrip_and_whole_stream_prediction(self):
        restored = json.loads(json.dumps(self.model))
        report = model.predict(restored)
        self.assertEqual(report["event_count"], 3)
        self.assertEqual(report["cursor_after"], 3)
        self.assertEqual([block["events"][0]["selected"] for block in report["blocks"]], [0, 1, 2])
        self.assertTrue(model.verify(restored, restored["baseline_order"], baseline())["verified"])

    def test_held_out_reorder_distinct_choices_and_uids(self):
        order = ["B#1", "A#1", "C#1"]
        report = model.verify(self.model, order, held_out())
        self.assertEqual(report["verified_events"], 3)
        self.assertFalse(report["insn_uid_used_as_identity"])
        self.assertEqual([b["events"][0]["selected"] for b in report["prediction"]["blocks"]], [1, 4, 5])

    def test_corrupt_selection_and_cursor_negative_controls(self):
        for key, value in (("selected", 4), ("cursor_after", 2)):
            trace = baseline()
            trace["events"][0][key] = value
            with self.subTest(key=key), self.assertRaisesRegex(model.ModelError, "selection/cursor"):
                model.calibrate(trace)

    def test_missing_event_fields_and_unsupported_event_are_not_omitted(self):
        for key in ("mode", "eligibility", "selected", "from"):
            trace = baseline()
            del trace["events"][1][key]
            with self.subTest(key=key), self.assertRaises(model.ModelError):
                model.calibrate(trace)
        for key, value in (("mode", "DImode"), ("constraint", "q")):
            trace = baseline()
            trace["events"][1][key] = value
            with self.subTest(key=key), self.assertRaisesRegex(model.ModelError, "unsupported"):
                model.calibrate(trace)

    def test_invalid_envelope_and_discontinuity(self):
        for key, value in (("events", []), ("errors", ["observer failed"]), ("pending", 1),
                           ("exit_codes", []), ("exit_codes", [False]), ("exit_codes", [0, 0])):
            trace = baseline()
            trace[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(model.ModelError):
                model.calibrate(trace)
        for key in ("events", "errors", "pending", "exit_codes"):
            trace = baseline()
            del trace[key]
            with self.subTest(missing=key), self.assertRaises(model.ModelError):
                model.calibrate(trace)
        trace = baseline()
        trace["events"][1]["cursor_before"] = 0
        with self.assertRaisesRegex(model.ModelError, "discontinuity"):
            model.calibrate(trace)

    def test_repeated_order_slots_use_slot_position(self):
        order = [0, 0] + list(range(1, 52))
        calibrated = model.calibrate(envelope([event("slots", [0, 1], 1, 0, 2, order)]))
        self.assertEqual(model.predict(calibrated)["cursor_after"], 2)
        self.assertEqual(model.predict(calibrated, start=2)["blocks"][0]["events"][0]["selected"], 1)

    def test_eligibility_filters_and_no_selection_reset(self):
        row = event("filters", list(range(8)) + [20], 0, 7, 8)
        state = row["eligibility"]
        state["fixed"][0] = 1
        state["call_used"][1] = 0
        state["live"] = [2, 3]
        row["excluded"] = [4, 5]
        state["frame_pointer_needed"] = 1
        calibrated = model.calibrate(envelope([row]))
        self.assertEqual(model.predict(calibrated)["blocks"][0]["events"][0]["selected"], 7)
        state["ever_live"][1] = 1
        row["selected"], row["cursor_after"] = 1, 2
        model.calibrate(envelope([row]))
        row = event("empty", [], 17, None, 0)
        self.assertEqual(model.predict(model.calibrate(envelope([row])))["cursor_after"], 0)
        row = event("unsupported-register", [8], 0, 8, 9)
        with self.assertRaisesRegex(model.ModelError, "register-mode"):
            model.calibrate(envelope([row]))

    def test_invalid_allocation_order_and_eligibility_shapes(self):
        for mutate in (lambda e: e["allocation_order"].pop(),
                       lambda e: e["allocation_order"].__setitem__(0, 53),
                       lambda e: e["eligibility"]["fixed"].pop(),
                       lambda e: e["eligibility"]["fixed"].__setitem__(0, 2),
                       lambda e: e["eligibility"].__setitem__("class_members", [0, 0]),
                       lambda e: e.__setitem__("excluded", [-1]),
                       lambda e: e.__setitem__("cursor_before", True)):
            trace = baseline()
            mutate(trace["events"][0])
            with self.assertRaises(model.ModelError):
                model.calibrate(trace)

    def test_candidate_redundant_evidence_corruption(self):
        trace = baseline()
        trace["events"][0]["candidates"] = [{"order_index": 0, "register": 0, "fixed": 1}]
        with self.assertRaisesRegex(model.ModelError, "changed fixed"):
            model.calibrate(trace)
        trace["events"][0]["candidates"] = [{"order_index": 0, "register": 1}]
        with self.assertRaisesRegex(model.ModelError, "slot/register"):
            model.calibrate(trace)

    def test_invalid_input_permutation_and_start(self):
        for order in ([], ["A#1", "A#1", "C#1"], ["A#1", "B#1", "missing"], "A#1"):
            with self.subTest(order=order), self.assertRaises(model.ModelError):
                model.predict(self.model, order)
        for start in (-1, 53, True, 0.0):
            with self.subTest(start=start), self.assertRaises(model.ModelError):
                model.predict(self.model, start=start)

    def test_changed_inputs_rejected_even_when_outcome_is_unchanged(self):
        mutations = (("eligibility", lambda e: e["eligibility"]["fixed"].__setitem__(52, 1)),
                     ("allocation_order", lambda e: e["allocation_order"].__setitem__(52, 51)),
                     ("excluded", lambda e: e["excluded"].append(52)),
                     ("from", lambda e: e.__setitem__("from", 1)),
                     ("to", lambda e: e.__setitem__("to", 2)))
        for label, mutate in mutations:
            trace = held_out()
            mutate(trace["events"][0])
            with self.subTest(label=label), self.assertRaisesRegex(model.ModelError, f"mechanism input {label}"):
                model.verify(self.model, ["B#1", "A#1", "C#1"], trace)

    def test_verify_missing_event_and_changed_order_controls(self):
        trace = held_out()
        trace["events"].pop()
        with self.assertRaisesRegex(model.ModelError, "event count"):
            model.verify(self.model, ["B#1", "A#1", "C#1"], trace)
        with self.assertRaisesRegex(model.ModelError, "function"):
            model.verify(self.model, self.model["baseline_order"], held_out())

    def test_actual_trace_must_preserve_present_provenance(self):
        trace = baseline()
        provenance = {"compiler_sha256": "compiler", "observer_sha256": "observer",
                      "image_id": "image", "effective_flags": ["-O3"]}
        trace["provenance"] = provenance
        calibrated = model.calibrate(trace)
        actual = held_out()
        with self.assertRaisesRegex(model.ModelError, "provenance"):
            model.verify(calibrated, ["B#1", "A#1", "C#1"], actual)
        actual["provenance"] = copy.deepcopy(provenance)
        self.assertTrue(model.verify(calibrated, ["B#1", "A#1", "C#1"], actual)["verified"])
        actual["provenance"]["effective_flags"] = ["-O2"]
        with self.assertRaisesRegex(model.ModelError, "provenance"):
            model.verify(calibrated, ["B#1", "A#1", "C#1"], actual)

    def test_search_full_product_fixed_positions_and_known_hit(self):
        family = {"groups": [["A#1", "B#1"]],
                  "targets": [{"block": "C#1", "event": 0, "selected": 5}]}
        report = model.search(self.model, family)
        self.assertEqual((report["denominator"], report["evaluated"], report["match_count"]), (2, 2, 1))
        self.assertTrue(report["exhaustive"])
        self.assertEqual(report["matches"][0]["order"], ["B#1", "A#1", "C#1"])
        self.assertGreaterEqual(report["wall_seconds"], 0)
        self.assertGreaterEqual(report["cpu_seconds"], 0)
        self.assertEqual(list(model._orders(list("ABCDE"), [["A", "C"], ["B", "D"]])),
                         [list("ABCDE"), list("ADCBE"), list("CBADE"), list("CDABE")])

    def test_no_match_and_incomplete_search_are_distinct(self):
        family = {"groups": [["A#1", "B#1", "C#1"]],
                  "targets": [{"block": "C#1", "event": 0, "selected": 7}]}
        exhaustive = model.search(self.model, family)
        self.assertEqual((exhaustive["status"], exhaustive["evaluated"], exhaustive["match_count"]),
                         ("no_match", 6, 0))
        family["limit"] = 2
        incomplete = model.search(self.model, family)
        self.assertEqual((incomplete["status"], incomplete["evaluated"], incomplete["denominator"]),
                         ("incomplete", 2, 6))
        self.assertFalse(incomplete["exhaustive"])

    def test_hits_retained_and_explicit_output_cap(self):
        family = {"groups": [["B#1", "C#1"]],
                  "targets": [{"block": "A#1", "event": 0, "selected": 0}]}
        self.assertEqual(len(model.search(self.model, family)["matches"]), 2)
        with patch.object(model, "OUTPUT_CAP", 1):
            report = model.search(self.model, family)
        self.assertEqual(report["match_count"], 2)
        self.assertEqual(report["output_cap"], 1)
        self.assertTrue(report["matches_truncated"])

    def test_invalid_family_controls(self):
        valid = {"groups": [["A#1", "B#1"]],
                 "targets": [{"block": "C#1", "event": 0, "selected": 5}]}
        for groups in ([], [[]], [["A#1", "A#1"]], [["missing"]],
                       [["A#1", "B#1"], ["B#1", "C#1"]]):
            family = dict(valid, groups=groups)
            with self.subTest(groups=groups), self.assertRaises(model.ModelError):
                model.search(self.model, family)
        for targets in ([], [{"block": "C#1", "selected": 5}],
                        [{"block": "C#1", "event": 1, "selected": 5}],
                        valid["targets"] * 2):
            with self.subTest(targets=targets), self.assertRaises(model.ModelError):
                model.search(self.model, dict(valid, targets=targets))
        with self.assertRaises(model.ModelError):
            model.search(self.model, dict(valid, limit=0))

    def test_repeated_function_occurrences_preserved_reorder_refused(self):
        trace = baseline()
        trace["events"][2]["function"] = "A"
        calibrated = model.calibrate(trace)
        self.assertEqual(calibrated["baseline_order"], ["A#1", "B#1", "A#2"])
        self.assertEqual(model.predict(calibrated)["event_count"], 3)
        with self.assertRaisesRegex(model.ModelError, "repeated function"):
            model.predict(calibrated, ["A#2", "B#1", "A#1"])

    def test_multievent_blocks_compose_every_transition(self):
        trace = baseline()
        trace["events"][1]["function"] = "A"
        calibrated = model.calibrate(trace)
        self.assertEqual(calibrated["baseline_order"], ["A#1", "C#1"])
        self.assertEqual(len(calibrated["blocks"][0]["events"]), 2)
        self.assertEqual(len(model.predict(calibrated)["blocks"][0]["events"]), 2)

    def test_serialized_model_tampering_rejected_at_all_api_entries(self):
        corrupt = copy.deepcopy(self.model)
        corrupt["blocks"][0]["transitions"][0]["events"][0]["selected"] = 4
        for operation in (lambda: model.predict(corrupt),
                          lambda: model.search(corrupt, {}),
                          lambda: model.verify(corrupt, corrupt["baseline_order"], baseline())):
            with self.assertRaisesRegex(model.ModelError, "corrupted model"):
                operation()

    def test_cli_json_outputs_failure_exit_and_optimized_validation(self):
        script = Path(model.__file__)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            trace, saved, order, actual, family, report = [root / f"{name}.json" for name in
                                                         ("trace", "model", "order", "actual", "family", "report")]
            trace.write_text(json.dumps(baseline()))
            order.write_text(json.dumps(["B#1", "A#1", "C#1"]))
            actual.write_text(json.dumps(held_out()))
            family.write_text(json.dumps({"groups": [["A#1", "B#1"]],
                                          "targets": [{"block": "C#1", "event": 0, "selected": 5}]}))
            commands = (("calibrate", trace, "--output", saved),
                        ("search", saved, family, "--output", report),
                        ("verify", saved, order, actual, "--output", report))
            for arguments in commands:
                result = subprocess.run([sys.executable, "-O", str(script), *map(str, arguments)],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(json.loads(report.read_text())["verified"])
            invalid = baseline()
            invalid["events"][0]["selected"] = 4
            trace.write_text(json.dumps(invalid))
            result = subprocess.run([sys.executable, "-O", str(script), "calibrate", str(trace)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn("selection/cursor mismatch", result.stderr)


if __name__ == "__main__":
    unittest.main()
