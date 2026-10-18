#!/usr/bin/env python3
"""Regression controls for refcheck's non-citation masking."""

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import refcheck


class ReferenceMasking(unittest.TestCase):
    def refs(self, text):
        return [(kind, number, line) for kind, number, line, _ in
                refcheck.refs_in("sample.md", text)]

    def test_complete_atd_commands_are_not_deviation_references(self):
        refs = self.refs("at&d0 AT&D1 At&D2 aT&d3")
        self.assertEqual(refs, [])

    def test_real_deviation_citations_near_commands_are_retained(self):
        refs = self.refs("AT&D0; see D0, then D1 and D77.")
        self.assertEqual(refs, [("D", "0", 1), ("D", "1", 1),
                                ("D", "77", 1)])

    def test_mixed_command_and_citation_on_wrapped_lines(self):
        refs = self.refs("AT&D0, see\n D0; AT&D1, see D1")
        self.assertEqual(refs, [("D", "0", 2), ("D", "1", 2)])

    def test_only_supported_complete_command_tokens_are_masked(self):
        refs = self.refs("XAT&D0 AT&D77 AT&D4 AT&D2suffix prefixAT&D3")
        self.assertEqual(refs, [("D", "0", 1), ("D", "77", 1),
                                ("D", "4", 1), ("D", "3", 1)])


if __name__ == "__main__":
    unittest.main()
