#!/usr/bin/env python3
"""Regression controls for refcheck's non-citation masking."""

from pathlib import Path
import sys
import builtins
import contextlib
import io
from unittest import mock
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import refcheck


class ReferenceMasking(unittest.TestCase):
    def refs(self, text):
        return [(kind, number, line) for kind, number, line, _ in
                refcheck.refs_in("sample.md", text)]

    def test_fixture_source_has_no_literal_deviation_citations(self):
        self.assertEqual(self.refs(Path(__file__).read_text()), [])

    def test_complete_atd_commands_are_not_deviation_references(self):
        refs = self.refs("at&d0 AT&D1 At&D2 aT&d3")
        self.assertEqual(refs, [])

    def test_real_deviation_citations_near_commands_are_retained(self):
        refs = self.refs(f"AT&D0; see D{0}, then D{1} and D{77}.")
        self.assertEqual(refs, [("D", "0", 1), ("D", "1", 1),
                                ("D", "77", 1)])

    def test_mixed_command_and_citation_on_wrapped_lines(self):
        refs = self.refs(f"AT&D0, see\n D{0}; AT&D1, see D{1}")
        self.assertEqual(refs, [("D", "0", 2), ("D", "1", 2)])

    def test_only_supported_complete_command_tokens_are_masked(self):
        refs = self.refs(f"XAT&D{0} AT&D{77} AT&D{4} AT&D2suffix prefixAT&D{3}")
        self.assertEqual(refs, [("D", "0", 1), ("D", "77", 1),
                                ("D", "4", 1), ("D", "3", 1)])


class CitationGatePolicy(unittest.TestCase):
    def test_citations_remain_gated_without_retired_registry_reads(self):
        real_open = builtins.open

        def guarded_open(path, *args, **kwargs):
            if "test/mutations/" in str(path):
                raise AssertionError("citation gate opened a retired registry")
            return real_open(path, *args, **kwargs)

        documents = {
            refcheck.FINDINGS: f"## F{1}. Known finding\n",
            refcheck.DEVIATIONS: "",
            "sample.md": f"See finding F{1}.",
        }
        with mock.patch.object(refcheck, "tracked", return_value=["sample.md"]), \
                mock.patch.object(refcheck, "read", side_effect=documents.get), \
                mock.patch.object(refcheck, "read_pending", return_value={}), \
                mock.patch("builtins.open", side_effect=guarded_open), \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(refcheck.check_dangling(), 0)
            documents["sample.md"] = f"See finding F{999999}."
            self.assertEqual(refcheck.check_dangling(), 1)


if __name__ == "__main__":
    unittest.main()
