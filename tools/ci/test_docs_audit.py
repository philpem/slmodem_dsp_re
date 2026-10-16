#!/usr/bin/env python3
"""Exercise the documentation audit CLI against small valid and broken trees."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

AUDIT = Path(__file__).with_name("docs_audit.py").resolve()
SOURCE_LINK = "[example](https://github.com/philpem/slmodem_dsp_re/blob/master/src/example.c)\n"


class DocumentationAudit(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="docs-audit-test-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for directory in ("src", "docs/maintainer", "build/docs-site"):
            (self.root / directory).mkdir(parents=True)
        self.write("src/example.c", "int example(void) { return 0; }\n")
        self.write("docs/maintainer/modules.md", SOURCE_LINK)
        self.write("README.md", "")
        self.write("CLAUDE.md", "")
        self.write("build/docs-site/index.html", '<a id="home" href="#home">Home</a>\n')

    def write(self, path, content):
        (self.root / path).write_text(content)

    def audit(self, expected_error=None):
        result = subprocess.run(
            [sys.executable, str(AUDIT)], cwd=self.root,
            text=True, capture_output=True,
        )
        output = result.stdout + result.stderr
        if expected_error is None:
            self.assertEqual(result.returncode, 0, output)
        else:
            self.assertEqual(result.returncode, 1, output)
            self.assertIn(expected_error, result.stderr, output)

    def test_valid_tree(self):
        self.audit()

    def test_empty_generated_page(self):
        self.write("build/docs-site/index.html", "")
        self.audit("generated site contains no local links")

    def test_missing_generated_index(self):
        (self.root / "build/docs-site/index.html").unlink()
        self.audit("build the manual before running this audit")

    def test_missing_document(self):
        self.write("README.md", "[missing](docs/absent.md)\n")
        self.audit("README.md: missing docs/absent.md")

    def test_missing_document_with_code_formatted_label(self):
        self.write("README.md", "[`missing`](docs/absent.md)\n")
        self.audit("README.md: missing docs/absent.md")

    def test_inline_code_link_example_is_ignored(self):
        self.write("README.md", "`[example](docs/absent.md)`\n")
        self.audit()

    def test_fenced_code_link_example_is_ignored(self):
        self.write("README.md", "```md\n[example](docs/absent.md)\n```\n")
        self.audit()

    def test_missing_html_fragment(self):
        self.write("build/docs-site/index.html", '<a href="#absent">Missing</a>\n')
        self.audit("index.html: #absent")

    def test_source_missing_from_catalogue(self):
        self.write("src/second.c", "int second(void) { return 0; }\n")
        self.audit("src/second.c")

    def test_duplicate_catalogue_entry(self):
        self.write("docs/maintainer/modules.md", SOURCE_LINK * 2)
        self.audit("src/example.c")

    def test_deleted_catalogued_source(self):
        (self.root / "src/example.c").unlink()
        self.audit("docs/maintainer/modules.md: missing")


if __name__ == "__main__":
    unittest.main()
