#!/usr/bin/env python3
"""Regression controls for refcheck's non-citation masking."""

from pathlib import Path
import sys
import unittest
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import refcheck


class ReferenceMasking(unittest.TestCase):
    def refs(self, text):
        return [(kind, number, line) for kind, number, line, _ in
                refcheck.refs_in("sample.md", text)]

    def test_tracked_scan_excludes_out_of_scope_tree(self):
        result = SimpleNamespace(stdout="docs/current.md\nre/private.c\n")
        with patch.object(refcheck.subprocess, "run", return_value=result) as run, \
                patch.object(refcheck.os.path, "exists", return_value=True):
            self.assertEqual(refcheck.tracked(), ["docs/current.md"])
        run.assert_called_once_with(
            ["git", "ls-files", "--", ".", ":(exclude)re/**"],
            capture_output=True, text=True)

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


if __name__ == "__main__":
    unittest.main()
