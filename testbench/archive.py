#!/usr/bin/env python3
"""archive.py -- copy the captures worth keeping to the ZFS store, and index them.

    archive.py --dry-run
    archive.py

WHY ANYTHING IS KEPT AT ALL.  `captures/` is gitignored on the principle that
the repository keeps the instruments and not the readings -- the analysis
scripts regenerate every number from the audio, so the audio is reproducible
input, not a result.  That is right for a batch nobody has cited.

It is WRONG for a capture a finding rests on.  `docs/findings.md` names
specific calls as evidence ("47 of 50 underflows in a representative call",
"cbase3 goes 7.5 -> 205.0 ms"), and a reader who cannot open that call cannot
check the claim.  Those are not readings any more; they are the provenance of
the record.  This copies exactly those to
/mnt/zfs/projects/softmodems/slmodem-re/captures and writes an index.

THE KEEP RULE, and it is mechanical rather than a matter of taste:

  1. Any capture prefix NAMED in claude_re/docs/*.md or in a testbench tool.
     If a finding or a script mentions it, it is evidence and it stays.
  2. Any prefix matching an explicit --also glob, for a batch whose calls are
     cited collectively rather than by name (an A/B is evidence as a SET; no
     individual call in it is quoted).

Everything else is a reading and stays only in the working tree, where it can
be deleted freely.

THE INDEX IS THE POINT.  A directory of 900 opaque files is not an archive.
`INDEX.tsv` gives, per capture: the prefix, the file count and bytes, the
sha256 of its slmodemd log (a cheap identity that survives a re-copy), and
WHICH document cites it.  It is written into the archive AND kept in the
repository, so the tree always knows what is in the store without mounting it.

LAYOUT is two levels, `<family>/<prefix>/`, where the family is the prefix up
to its first dash -- `ab149-1901-a0-1` lands in `ab149/ab149-1901-a0-1/`.  A
flat directory of nine hundred files is not an archive either; this keeps a
batch together, which is how batches are read.
"""

import argparse
import glob
import hashlib
import os
import re
import shutil
import sys
from collections import defaultdict

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAPS = os.path.join(REPO, "testbench", "captures")
DEST = "/mnt/zfs/projects/softmodems/slmodem-re/captures"
INDEX_IN_REPO = os.path.join(REPO, "testbench", "records", "archive-INDEX.tsv")


def cited_prefixes(prefixes):
    """Which prefixes are named in the durable record or in a tool."""
    cited = defaultdict(set)
    srcs = (glob.glob(os.path.join(REPO, "claude_re", "docs", "*.md")) +
            glob.glob(os.path.join(REPO, "testbench", "*.sh")) +
            glob.glob(os.path.join(REPO, "testbench", "*.py")))
    me = os.path.basename(__file__)
    for s in srcs:
        # A tool does not count as evidence for itself.  This file's own
        # docstring names an example capture, and without this exclusion that
        # capture is indexed as "cited by archive.py" -- a citation loop.
        if os.path.basename(s) == me:
            continue
        try:
            txt = open(s, errors="replace").read()
        except OSError:
            continue
        for p in prefixes:
            # Word-boundaried on both sides, and '-' is NOT a word character
            # to re.  Without the explicit guards, "base-2" matches inside
            # "base-2b" and drags unrelated batches in.
            if re.search(r"(?<![\w-])" + re.escape(p) + r"(?![\w-])", txt):
                cited[p].add(os.path.basename(s))
    return cited


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--dest", default=DEST)
    ap.add_argument("--also", action="append", default=[],
                    help="glob of prefixes to keep regardless of citation, "
                         "for a batch cited as a set (e.g. 'ab149-*')")
    args = ap.parse_args()

    if not os.path.isdir(CAPS):
        sys.exit("archive: no %s" % CAPS)
    prefixes = sorted({f.split(".")[0] for f in os.listdir(CAPS)
                       if len(f.split(".")[0]) >= 4})

    cited = cited_prefixes(prefixes)
    for pat in args.also:
        for p in prefixes:
            if glob.fnmatch.fnmatch(p, pat):
                cited[p].add("(batch: %s)" % pat)

    rows, total = [], 0
    for p in sorted(cited):
        files = sorted(glob.glob(os.path.join(CAPS, p + ".*")))
        if not files:
            continue
        size = sum(os.path.getsize(f) for f in files)
        log = next((f for f in files if f.endswith("slmodemd.log")), None)
        rows.append((p, len(files), size, log, sorted(cited[p])))
        total += size

    print("captures on disk      : %d prefixes" % len(prefixes))
    print("kept (cited or batch) : %d prefixes, %.2f GB"
          % (len(rows), total / 1e9))
    print("destination           : %s%s"
          % (args.dest, "  [DRY RUN]" if args.dry_run else ""))

    if args.dry_run:
        for p, n, size, _, why in rows[:10]:
            print("   %-24s %3d files %8.1f MB  <- %s"
                  % (p, n, size / 1e6, ",".join(why)))
        print("   ... and %d more" % max(0, len(rows) - 10))
        return 0

    def family(pfx):
        """Batch a capture belongs to: the prefix up to its first dash.

        `ab149-1901-a0-1` -> `ab149`, `asym-3` -> `asym`, `cbase3` -> `cbase3`.
        Deliberately dumb: a wrong grouping is a cosmetic annoyance, whereas a
        clever one that changes as names evolve would move files between runs
        and break the index's paths.
        """
        return pfx.split("-")[0] if "-" in pfx else pfx

    os.makedirs(args.dest, exist_ok=True)
    os.makedirs(os.path.dirname(INDEX_IN_REPO), exist_ok=True)
    index = ["# capture archive index -- written by testbench/archive.py",
             "# path\tprefix\tfiles\tbytes\tsha256(slmodemd.log)\tcited_by"]
    for p, n, size, log, why in rows:
        outdir = os.path.join(args.dest, family(p), p)
        os.makedirs(outdir, exist_ok=True)
        for f in glob.glob(os.path.join(CAPS, p + ".*")):
            d = os.path.join(outdir, os.path.basename(f))
            if not (os.path.exists(d) and os.path.getsize(d) == os.path.getsize(f)):
                shutil.copy2(f, d)
        index.append("%s\t%s\t%d\t%d\t%s\t%s"
                     % (os.path.join(family(p), p), p, n, size,
                        sha256_of(log)[:16] if log else "-", ",".join(why)))
    body = "\n".join(index) + "\n"
    open(os.path.join(args.dest, "INDEX.tsv"), "w").write(body)
    open(INDEX_IN_REPO, "w").write(body)
    print("wrote %s and %s" % (os.path.join(args.dest, "INDEX.tsv"),
                               INDEX_IN_REPO))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
