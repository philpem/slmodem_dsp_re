#!/usr/bin/env python3
"""Audit current documentation targets, module coverage and generated links.

Run from the repository root after `python -m mkdocs build --strict`.
External URLs and pinned historical revisions are outside the current-path
check. Pre-existing untracked build-artifact links are reported separately.
"""

from collections import Counter
from html.parser import HTMLParser
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit

REPO = "https://github.com/philpem/slmodem_dsp_re/"
LINK = re.compile(r"\[[^\]\n]+\]\(([^\s)]+)\)")


def prose(text):
    text = re.sub(r"(?ms)^```.*?^```\s*", "", text)
    return re.sub(r"`+[^`\n]+`+", "", text)


def target_path(root, page, target):
    if target.startswith(REPO):
        match = re.match(r"(?:blob|tree)/master/(.+)", target[len(REPO):])
        return root / match[1].split("#")[0] if match else None
    if re.match(r"[A-Za-z]+:|#|//", target):
        return None
    return page.parent / unquote(target.split("#")[0])


def catalogue_gaps(sources, links):
    counts = Counter(links)
    return sources - counts.keys(), {p for p, n in counts.items() if n > 1}, counts.keys() - sources


class Page(HTMLParser):
    def __init__(self, text):
        super().__init__()
        self.ids = set()
        self.links = []
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            self.ids.add(attrs["id"])
        if tag == "a" and "href" in attrs:
            self.links.append(attrs["href"])


def main():
    root = Path.cwd()
    errors = []
    # Historical scratch artifacts were never tracked (baseline 4169cb3b).
    old_artifacts = {
        'build/playbook-ctor-tu-position/results.json',
        'build/playbook-demod-tdx/results.json',
        'build/playbook-fdsp-dprt-r2/results.json',
        'build/playbook-fdsp-dprt/results.json',
        'build/playbook-floatfir-ctor/results.json',
        'build/playbook-fse-zeros-v92scr/results.json',
        'build/playbook-masks-v22fp-w2/results.json',
        'build/playbook-queue-segptr/results.json',
        'build/playbook-rcfixed-p4m/results.json',
        'build/playbook-v34-residual/results.json',
        'build/playbook-v90jd-pos/results.json',
        'build/playbook-v92cp-cross/results.json',
    }
    files = sorted(Path("docs").rglob("*.md")) + [Path("README.md"), Path("CLAUDE.md")]
    checked = 0
    skipped = 0
    for page in files:
        for match in LINK.finditer(prose(page.read_text())):
            target = match[1]
            if page == Path("docs/findings.md") and target in old_artifacts:
                skipped += 1
                continue
            path = target_path(root, page, target)
            if path is not None:
                checked += 1
                if not path.exists():
                    errors.append(f"{page}: missing {target}")
    print(f"Historical build-artifact links outside scope: {skipped}.")
    print(f"Documentation targets: {checked} checked across {len(files)} Markdown files, "
          f"{len(errors)} missing.")

    sources = {str(p) for p in Path("src").rglob("*") if p.suffix in (".c", ".cpp", ".S")}
    catalogue = Path("docs/maintainer/modules.md").read_text()
    links = re.findall(re.escape(REPO) + r"blob/master/(src/[^)#]+)", catalogue)
    missing, duplicates, unknown = catalogue_gaps(sources, links)
    print(f"Module catalogue: {len(sources)} production inputs; {len(links)} links; "
          f"{len(missing)} missing, {len(duplicates)} duplicated, {len(unknown)} unknown.")
    errors.extend(sorted(missing | duplicates | unknown))
    if not sources:
        errors.append("source inventory is empty")
    # Exercise the same decisions used above, with known missing inputs.
    missing_control = bool(sources and catalogue_gaps(sources, links[1:])[0])
    planted = target_path(root, Path("docs/README.md"), "__planted_missing_document__.md")
    target_control = planted is not None and not planted.exists()
    controls = int(missing_control) + int(target_control)
    print(f"Negative controls: {controls}/2 detected (missing module and missing target).")
    if controls != 2:
        errors.append("documentation negative control failed")

    site = Path("build/docs-site")
    if not (site / "index.html").is_file():
        errors.append("build the manual before running this audit")
    pages = {p.resolve(): Page(p.read_text()) for p in site.rglob("*.html")}
    html_checked = 0
    bad_html = []
    for page, doc in pages.items():
        for href in doc.links:
            url = urlsplit(href)
            if url.scheme or url.netloc:
                continue
            if url.path.startswith("/"):
                path = (site / unquote(url.path).lstrip("/")).resolve()
            elif url.path:
                path = (page.parent / unquote(url.path)).resolve()
            else:
                path = page
            if path.is_dir():
                path /= "index.html"
            html_checked += 1
            if not path.exists() or (url.fragment and path in pages
                                    and unquote(url.fragment) not in pages[path].ids):
                bad_html.append(f"{page}: {href}")
    print(f"Generated HTML: {html_checked} local targets/fragments checked across "
          f"{len(pages)} pages; {len(bad_html)} failures.")
    errors.extend(bad_html)
    for error in errors:
        print(error, file=sys.stderr)
    return int(bool(errors))


if __name__ == "__main__":
    sys.exit(main())
