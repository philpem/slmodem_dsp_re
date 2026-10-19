#!/usr/bin/env python3
"""Structural similarity between the blob's function and ours, as a MEASURE.

WHY A MEASURE AND NOT A GRADE

`byteident.py` answers "exact or not", and its grade 1 rejects any difference
in instruction COUNT -- correctly, because a renaming cannot add or remove an
instruction.  That makes it a poor triage tool for a SIZE miss: a function
with two extra instructions and one that shares nothing both read as REJECT,
and neither says how far from the shape we are.

`compare.py` compares MNEMONICS with operands dropped, so two functions that
store the same constants to different offsets read as the same code, and it
reports only per-symbol identical-or-not.  `samesize.py` counts the
same-size/different-instruction bucket but does not measure a near miss.

This tool reports, per symbol:

    longest common contiguous run   the strongest evidence: a whole block
                                    emitted identically, in the same order
    LCS                             longest common SUBSEQUENCE: the largest
                                    ordered set of instructions present in
                                    both when instructions may be skipped.
                                    Order-preserving, so insertions and
                                    deletions are tolerated; a pure
                                    reordering is not.
    LCS %                           LCS / length of the longer sequence.  A
                                    summary only -- quote the run beside it.

Register names are canonicalised to `R`, condition codes and call/jump
targets are kept as their own classes (a call to a DIFFERENT function must not
match), and section-relative relocation addends are masked because a per-TU
`.rodata`/`.bss` addend is not comparable with the merged object's.  Small
displacements and immediates are KEPT: those carry the structure we are
trying to recover.

ALIGNMENT PADDING IS STRIPPED, as `byteident.py --near` does.  GCC fills loop
heads with `nop`, `lea 0x0(%esi,%eiz,1),%esi` and `mov %esi,%esi`, and counting
those as instructions turns a scheduling difference into a shape difference.
The number stripped is reported.  `--keep-padding` disables it.

WHAT THIS IS NOT

Not a gate and not a ratchet.  LCS falls when the compiler schedules two
independent statements differently, even though the source recovery is
correct -- the same reason `compare.py --ratchet` is deliberately out of
`make phase`.  Byte identity remains the target and is taken when the
candidate space is exhausted and exactly one element maps onto the object
(AGENTS.md).  Use this to CHOOSE a target and to record how much of a
function's shape a recovery restored, never as a score to hill-climb.

    make structsim S=v8_tone_detect
    python3 tools/toolchain/structsim.py v8_tone_detect v8_getbit
    python3 tools/toolchain/structsim.py --worst 40

Denominator: the symbols the blob and TC_OUT both define.  Every verdict line
prints the counts; the tool refuses on an empty or stale object tree, because
a similarity of zero symbols renders as a clean run (findings F2400, F2401).

    BLOB=/abs/path/to/dsplibs.o TC_OUT=build/tc_out \
        python3 tools/toolchain/structsim.py --worst 40
"""

import argparse
import difflib
import functools
import glob
import multiprocessing
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

# One parser, one notion of padding, one denominator: byteident owns all
# three, and this tool must not grow a second opinion about them.
import byteident                                                      # noqa: E402

REGS = re.compile(
    r"%(?:e?(?:ax|bx|cx|dx|si|di|sp|bp)|[abcd][lh]|eiz|st\([0-7]\))")
SECTION_ADDEND = re.compile(r"\('section', '([^']+)', \d+\)")
SYMBOL_TARGET = re.compile(r"\('symbol', '([^']+)', -?\d+\)")
RELOC_CLASS = re.compile(r"R_386_\w+")


@functools.lru_cache(maxsize=None)
def blob_sizes():
    """nm on the merged object is ~0.5 s and --worst would otherwise run it once
    per symbol.  The blob cannot change during a run."""
    return byteident.sizes(byteident.BLOB)


def _branch_class(mn):
    """call / jmp / conditional jump, or None for an ordinary instruction."""
    if mn == "call" or mn.startswith("call"):
        return "call"
    if mn == "jmp" or mn.startswith("ljmp"):
        return "jmp"
    if len(mn) > 1 and mn[0] == "j" and mn not in ("jecxz", "jcxz"):
        return "condjump"
    if mn in ("loop", "loope", "loopne"):
        return "condjump"
    return None


def canon(mn, ops):
    """One instruction, canonicalised for a register-agnostic comparison."""
    ops = ops.strip()
    cls = _branch_class(mn)
    if cls is not None:
        # Keep WHICH function is called; drop the relative displacement.
        m = SYMBOL_TARGET.search(ops)
        return "%s sym:%s" % (cls, m.group(1)) if m else "%s sym:?" % cls
    #
    # A section-relative address is `(section, name, addend)` and the addend
    # is a per-TU layout artefact: the merged object's `.rodata` does not
    # start where a single TU's does.  Mask it, but keep the section name and
    # keep symbol addends, which encode `a[1]` versus `a[0]`.
    #
    ops = SECTION_ADDEND.sub(r"('section', '\1')", ops)
    ops = RELOC_CLASS.sub("RELOC", ops)
    ops = REGS.sub("R", ops)
    return "%s %s" % (mn, ops)


def code(path, sym, keep_padding=False):
    """Canonical instruction list, padding stripped unless asked to keep it."""
    rows = byteident.insns(path, sym)
    out, padded = [], 0
    for mn, ops in rows:
        if not keep_padding and byteident._padding(mn, ops):
            padded += 1
            continue
        out.append(canon(mn, ops))
    return out, padded


def lcs_len(a, b):
    """Exact longest common subsequence length (order preserved, gaps allowed)."""
    if not a or not b:
        return 0
    if len(a) < len(b):
        a, b = b, a
    prev = [0] * (len(b) + 1)
    for x in a:
        cur = [0]
        for j, y in enumerate(b, 1):
            if x == y:
                cur.append(prev[j - 1] + 1)
            else:
                p, c = prev[j], cur[j - 1]
                cur.append(p if p >= c else c)
        prev = cur
    return prev[-1]


def longest_run(a, b):
    """Longest common CONTIGUOUS run.  difflib finds it in one pass."""
    if not a or not b:
        return 0
    sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
    return sm.find_longest_match(0, len(a), 0, len(b)).size


def unmatched(a, b):
    """Alignment leftovers, for the single-symbol report."""
    sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
    only_a, only_b = [], []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag in ("replace", "delete"):
            only_a += a[i1:i2]
        if tag in ("replace", "insert"):
            only_b += b[j1:j2]
    return only_a, only_b


def ours_index():
    """symbol -> [defining objects], sorted; COMDAT copy is scored as well."""
    index = {}
    for obj in sorted(glob.glob(os.path.join(byteident.OURS, "*.o"))):
        for sym in byteident.sizes(obj):
            index.setdefault(sym, []).append(obj)
    return index


def score(blob_sym, blob_code, obj, keep_padding):
    ours_code, padded_o = code(obj, blob_sym, keep_padding)
    run = longest_run(blob_code, ours_code)
    l = lcs_len(blob_code, ours_code)
    longer = max(len(blob_code), len(ours_code))
    return {
        "blob": len(blob_code),
        "ours": len(ours_code),
        "run": run,
        "lcs": l,
        "pct": 100.0 * l / longer if longer else 0.0,
        "padded_ours": padded_o,
        "code": ours_code,
    }


def require_objects():
    """Refuse to measure nothing, and refuse to measure a stale tree."""
    index = ours_index()
    if not index:
        sys.exit("structsim.py: no objects in %s -- run `make tc` first."
                 % byteident.OURS)
    stale_src, stale_obj = byteident._staleness()
    if stale_src:
        sys.exit(
            "structsim.py: %s IS STALE; every similarity below would describe\n"
            "  a tree that no longer exists.\n\n"
            "    newest source : %s\n"
            "    newest object : %s\n\n"
            "  Run:  make tc\n"
            % (byteident.OURS, stale_src, stale_obj))
    return index


def report(sym, index, keep_padding, verbose=True):
    """Score one symbol against the blob; worst COMDAT copy wins."""
    blob_all = blob_sizes()
    if sym not in blob_all:
        sys.exit("structsim.py: the blob does not define %s" % sym)
    if sym not in index:
        sys.exit("structsim.py: no object in %s defines %s"
                 % (byteident.OURS, sym))
    blob_code, padded_b = code(byteident.BLOB, sym, keep_padding)
    best = None
    for obj in index[sym]:
        s = score(sym, blob_code, obj, keep_padding)
        if best is None or s["pct"] < best["pct"]:
            best, best["obj"] = s, obj
    best["padded_blob"] = padded_b
    if not verbose:
        return best
    print("%s" % sym)
    print("  blob %d insns, ours %d insns   (%d/%d alignment rows stripped)"
          % (best["blob"], best["ours"], padded_b, best["padded_ours"]))
    print("  longest common contiguous run : %d insns" % best["run"])
    print("  LCS                           : %d (%.1f%% of longer)"
          % (best["lcs"], best["pct"]))
    print("  object                        : %s" % best["obj"])
    if len(index[sym]) > 1:
        print("  %d defining object(s); worst copy scored (byteident COMDAT rule)"
              % len(index[sym]))
    only_b, only_o = unmatched(blob_code, best["code"])
    print("  blob-only (%d):" % len(only_b))
    for x in only_b[:20]:
        print("     B %s" % x)
    print("  ours-only (%d):" % len(only_o))
    for x in only_o[:20]:
        print("     O %s" % x)
    return best


_RANK = {}


def _rank_one(sym):
    """Worker: one symbol, worst COMDAT copy.  Runs in a forked child."""
    index = _RANK["index"]
    keep_padding = _RANK["keep"]
    max_cells = _RANK["max_cells"]
    try:
        blob_code, _ = code(byteident.BLOB, sym, keep_padding)
    except Exception:
        return ("skip", sym, "unreadable")
    best = None
    for obj in index[sym]:
        try:
            ours_code, _ = code(obj, sym, keep_padding)
        except Exception:
            return ("skip", sym, "unreadable")
        if len(blob_code) * len(ours_code) > max_cells:
            return ("skip", sym, "too large")
        run = longest_run(blob_code, ours_code)
        l = lcs_len(blob_code, ours_code)
        longer = max(len(blob_code), len(ours_code))
        pct = 100.0 * l / longer if longer else 0.0
        if best is None or pct < best[0]:
            best = (pct, run, len(blob_code), len(ours_code), sym)
    return best


def worst(index, count, keep_padding, max_cells):
    """Rank symbols by LCS% ascending: the least structurally right first."""
    blob = blob_sizes()
    common = sorted(k for k in index if k in blob)
    if not common:
        sys.exit("structsim.py: blob and %s share NO symbols; the denominator "
                 "is zero, which is not a score." % byteident.OURS)
    #
    # ONE objdump INVOCATION PER SYMBOL PER SIDE is the cost here (~3,700 of
    # them), and it is almost all ELF parsing.  Fork the workers AFTER the
    # index and the options are in place so each child inherits both; the LCS
    # itself is CPU-bound and parallelises cleanly.
    #
    _RANK["index"], _RANK["keep"], _RANK["max_cells"] = index, keep_padding, max_cells
    jobs = max(1, min(os.cpu_count() or 1, 8))
    rows, skipped = [], []
    try:
        with multiprocessing.Pool(jobs) as pool:
            results = pool.imap_unordered(_rank_one, common, chunksize=4)
            for item in results:
                if item is None:
                    continue
                if item[0] == "skip":
                    skipped.append((item[1], item[2]))
                else:
                    rows.append(item)
    except (OSError, ValueError):
        for sym in common:
            item = _rank_one(sym)
            if item is None:
                continue
            if item[0] == "skip":
                skipped.append((item[1], item[2]))
            else:
                rows.append(item)
    rows.sort(key=lambda r: (r[0], -r[2]))
    print("  %d of %d shared symbol(s) scored; %d skipped (max %d cells)"
          % (len(rows), len(common), len(skipped), max_cells))
    print("  %-6s %5s %6s %6s   %s" % ("LCS%", "run", "blob", "ours", "symbol"))
    for pct, run, nb, no, sym in rows[:count]:
        print("  %5.1f%% %5d %6d %6d   %s" % (pct, run, nb, no, sym[:66]))
    if skipped:
        kinds = {}
        for _, why in skipped:
            kinds[why] = kinds.get(why, 0) + 1
        print("  skipped: " + ", ".join("%d %s" % (v, k)
                                        for k, v in sorted(kinds.items())))
    return 0


def self_test():
    """Prove the metric fires, and prove it distinguishes what it must."""
    checks = []

    def check(label, cond):
        checks.append((label, bool(cond)))

    base = ["mov R,R", "add R,R", "cmp R,R", "jle A", "ret "]
    check("identical sequence scores 100%",
          lcs_len(base, base) == len(base)
          and longest_run(base, base) == len(base))
    inserted = base[:2] + ["xor R,R"] + base[2:]
    check("insertion keeps LCS, breaks the contiguous run",
          lcs_len(base, inserted) == len(base)
          and longest_run(base, inserted) == 3)
    swapped = ["add R,R", "mov R,R", "cmp R,R", "jle A", "ret "]
    check("swap is penalised by both",
          lcs_len(base, swapped) == len(base) - 1
          and longest_run(base, swapped) == 4 - 1)

    # Registers differ, structure does not.
    c1 = canon("mov", "0x14(%ebp),%eax")
    c2 = canon("mov", "0x14(%esp),%ebx")
    c3 = canon("mov", "0x1c(%esp),%ebx")
    check("register names are ignored", c1 == c2)
    check("displacements are NOT ignored", c1 != c3)

    # A call to a different function must not match; its displacement must go.
    k1 = canon("call", ".+70 @R_386_PC32:('symbol', 'v8_mpyint', -4)")
    k2 = canon("call", ".+393 @R_386_PC32:('symbol', 'v8_mpyint', -4)")
    k3 = canon("call", ".+70 @R_386_PC32:('symbol', 'v8_absfn', -4)")
    check("call displacement is ignored", k1 == k2)
    check("call target is kept", k1 != k3)
    check("conditional jump is its own class",
          canon("jle", ".+64") != canon("call", ".+64"))

    # Section-relative addends differ per TU; symbol addends do not.
    s1 = canon("movswl", "@R_386_32:('section', '.rodata', 5722)(%ebx,%ebx,1),%edx")
    s2 = canon("movswl", "@R_386_32:('section', '.rodata', 6)(%ebx,%ebx,1),%edx")
    d1 = canon("movswl", "@R_386_32:('symbol', 'a', 2)(%ebx,%ebx,1),%ecx")
    d2 = canon("movswl", "@R_386_32:('symbol', 'a', 0)(%ebx,%ebx,1),%ecx")
    check("section addend is masked", s1 == s2)
    check("symbol addend is kept", d1 != d2)

    bad = sum(not ok for _, ok in checks)
    for label, ok in checks:
        print("  %s  %s" % ("ok  " if ok else "FAIL", label))
    print("\n  %d self-test case(s), %d failure(s)" % (len(checks), bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("symbols", nargs="*", metavar="SYMBOL")
    ap.add_argument("--worst", type=int, default=0, metavar="N",
                    help="rank the N shared symbols with the LOWEST LCS%%")
    ap.add_argument("--keep-padding", action="store_true",
                    help="count nop/self-move/lea padding instead of stripping it")
    ap.add_argument("--max-cells", type=int, default=4_000_000, metavar="N",
                    help="--worst skips pairs whose LCS table would exceed N cells")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the metric accepts and rejects known shapes")
    a = ap.parse_args()

    if a.self_test:
        return self_test()
    index = require_objects()
    if a.worst:
        return worst(index, a.worst, a.keep_padding, a.max_cells)
    if not a.symbols:
        ap.error("give one or more symbols, or --worst N")
    for sym in a.symbols:
        report(sym, index, a.keep_padding)
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
