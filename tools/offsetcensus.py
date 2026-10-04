#!/usr/bin/env python3
"""Census of raw-offset object accesses for issue #260.

WHY THIS EXISTS

Issue #260 is the backlog of sites where an object's layout is expressed as
addresses rather than typed members:

    struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
    t3c_putp(obj, T41_PTR_AA70, (char *)obj + 0xa9dc);
    ((unsigned char *)&tx->cfg)[V21TX_OBJ_FLAGS] |= V21TX_FLAG;

The issue's table is a lexical witness inventory -- function/file pairs --
which is the right unit for "which functions still do this" and the wrong
unit for planning the cleanup, because the cleanup proceeds by OWNER
STRUCTURE: one struct recovered and typed per wave, with all of its raw
accesses converted at once.  This tool re-keys the same evidence by
(owner shape, resolved offset, access width) so a wave's denominator is a
count that moves as its sites convert, and so the access widths recovered
here are the field-width evidence the later waves define members from.

It finds three shapes, named for the conversion each needs:

  castadd   a pointer cast over `base + offset` -- the layout is an address
  helper    a call to a known offset accessor (`t3c_getb`, `hs_put`, the
            T3M_*/T4_* accessor macros) -- the layout hides behind an
            interface, and the offset CONSTANT is what to type
  byteview  a constant-indexed view through a byte pointer -- often a
            legitimate scalar low/upper-byte alias (issue #260's review
            boundary), so recorded separately rather than presumed a defect

None of these is a defect by itself.  docs/cleanup.md F3a and issue #260's
closing paragraph both say some are correct as written -- a byte view of a
typed member, a documented one-past-member access -- and that no struct
overlay or retype happens without the blob's own evidence.  This is the
triage ledger, not a gate: it ranks and denominates, it does not convict.

Resolving an offset constant needs the #define table of the file that uses
it, so defines are collected per file plus its local `#include "..."`
closure.  Constants that do not fold (loop indices, `+ 2 * i`) are recorded
as `dynamic` and kept, because a dynamic offset in an accessor call is
still a raw access the cleanup must reach.

The tool reports its denominators on every run: files scanned, regions
scanned (F134/F2400: a scanner that prints nothing is indistinguishable
from a broken one), sites by shape, folded vs dynamic offsets, and the
per-owner offset groups that become the wave ledger.

Usage:
    tools/offsetcensus.py                       # summary by owner shape
    tools/offsetcensus.py --json OUT            # full site ledger
    tools/offsetcensus.py --markdown OUT        # issue-ready table
    tools/offsetcensus.py --groups              # wave ledger by offset
    tools/offsetcensus.py --self-test           # fixture validation
"""

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIRS = [ROOT / "src", ROOT / "include" / "dsplib"]

#
# Offset accessor helpers with a fixed width in their name.  The suffix map
# is the accessor's own contract: b = byte, i = int, p = pointer, bare =
# short (hs_get/tx1_get move shorts -- see v34hstx1_arms.h and V34hshak.c).
# Anything not here and not an auto-detected accessor macro is skipped
# rather than guessed at; the run prints how many helper names it knew.
#
HELPER_SUFFIX = {
    "getb": 1, "putb": 1, "geti": 4, "puti": 4, "putp": 4, "getp": 4,
    "get": 2, "put": 2, "setstate": 2, "record": 4, "recget": 2,
    "recput": 2, "recputi": 4, "int": 4, "word": 4,
}
HELPER_NAMES = {
    "t3c_getb", "t3c_putb", "t3c_geti", "t3c_puti", "t3c_putp",
    "t41_getp", "t44_record", "t44_recget", "t44_recput", "t44_recputi",
    "preemp_get", "session_int", "session_ptr", "dp_rxget", "dp_rxput",
    "paramShort", "paramWord", "hs_get", "hs_put", "hs_setstate",
    "tx1_get", "tx1_put", "tx1_get_int", "tx1_put_int",
}
HELPER_CALL = re.compile(
    r"\b([A-Za-z_]\w*)\s*\(\s*([A-Za-z_][\w\-\.>\[\]\*]*)\s*"
    r"(?:,\s*([^,()]+?)\s*[,)]|[\s)])")

SCALAR_W = {
    "char": 1, "signed char": 1, "unsigned char": 1,
    "short": 2, "signed short": 2, "unsigned short": 2,
    "int": 4, "signed int": 4, "unsigned int": 4, "long": 4,
    "float": 4, "double": 8, "void": 4,
}

#
# A pointer cast: `(unsigned short *)`, `(const struct v34_receiver *)`,
# `(void **)`.  Captured non-greedily so several casts on one line each
# match; the offset scan starts after whichever cast precedes a `+`.
#
CAST = re.compile(r"\(\s*(?:const\s+|volatile\s+|signed\s+|unsigned\s+)*"
                  r"([A-Za-z_]\w*(?:\s+[A-Za-z_]\w*)*)\s*([\*]+)\s*\)")

OFFSET_ATOM = r"(?:0[xX][0-9a-fA-F]+|\d+u?|[A-Z][A-Z0-9_]{2,})"
#
# `+ OFF`, `+ OFF + OFF2`, `+ OFF * 3`, `+ OFF + 2 * i` -- the foldable
# prefix is consumed greedily and a trailing dynamic term leaves the whole
# offset `dynamic`.
#
ADDEND = re.compile(
    r"\+\s*(" + OFFSET_ATOM + r"(?:\s*[-+*]\s*(?:" + OFFSET_ATOM +
    r"|[A-Za-z_]\w*|\d+u?))*)")

#
# `tbl[OFF]` but also `tbl[OFF + k]` -- a constant-led byte offset with a
# possible dynamic tail, which folds to None and is kept as a site.
#
BYTEVIEW = re.compile(
    r"\[\s*(" + OFFSET_ATOM + r"(?:\s*[-+]\s*(?:" + OFFSET_ATOM +
    r"|[A-Za-z_]\w*))?)\s*\]")

DEFINE_OBJ = re.compile(r"^#define\s+([A-Za-z_]\w*)\s+(.+?)(?:\s*/\*|\s*$)")
DEFINE_FN = re.compile(r"^#define\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s+(.+?)(?:\s*/\*|\s*$)")
INCLUDE_LOCAL = re.compile(r'^#include\s+"([^"]+)"')

NUM = re.compile(r"^(0[xX][0-9a-fA-F]+|\d+)u?$")


def strip_comments_strings(text):
    """Blank out comments and string/char literals, preserving newlines."""
    out = []
    i, n = 0, len(text)
    state = None  # None | 'line' | 'block' | 'str' | 'chr'
    while i < n:
        c = text[i]
        two = text[i:i + 2]
        if state is None:
            if two == "//":
                state = "line"; i += 2; continue
            if two == "/*":
                state = "block"; out.append("  "); i += 2; continue
            if c == '"':
                state = "str"; out.append(" "); i += 1; continue
            if c == "'":
                state = "chr"; out.append(" "); i += 1; continue
            out.append(c); i += 1
        elif state == "line":
            if c == "\n":
                state = None; out.append("\n")
            i += 1
        elif state == "block":
            if two == "*/":
                state = None; i += 2
            else:
                out.append("\n" if c == "\n" else " "); i += 1
        else:  # str / chr
            if c == "\\":
                out.append("  "); i += 2; continue
            if (state == "str" and c == '"') or (state == "chr" and c == "'"):
                state = None
            out.append("\n" if c == "\n" else " "); i += 1
    return "".join(out)


def fold(expr, defines, depth=0):
    """Constant-fold an offset expression.  Returns int or None (dynamic)."""
    if depth > 8 or expr is None:
        return None
    e = expr.strip().rstrip(",")
    if not e:
        return None
    # Substitute identifiers from the define table, innermost-out.
    def sub(m):
        name = m.group(0)
        if name in defines:
            return "(" + defines[name] + ")"
        return name
    e = re.sub(r"\b([A-Za-z_]\w*)\b", sub, e)
    if not re.fullmatch(r"[0-9a-fA-FxXulUL+\-*()<> \t]+", e):
        return None
    e = re.sub(r"[uUlL]+\b", "", e)
    try:
        return int(eval(e, {"__builtins__": {}}, {}))  # noqa: S307 - charset-checked
    except Exception:
        return None


def base_shape(chunk):
    """Reduce a base expression to a comparable shape plus its identifier."""
    c = chunk.strip()
    c = re.sub(r"\s+", "", c)
    if c.startswith("&"):
        c = c[1:]
    m = re.search(r"([A-Za-z_]\w*)((->|\.)\w+)?$", c)
    if not m:
        return ("other", c)
    kind = "member" if m.group(2) else "ident"
    return (kind, m.group(1) + (m.group(2) or ""))


def collect_defines(text):
    """Object-like defines (foldable values only) and accessor macros."""
    defines, accessors = {}, {}
    for line in text.splitlines():
        m = DEFINE_OBJ.match(line)
        if m and "(" not in m.group(1):
            if fold(m.group(2), defines) is not None:
                defines[m.group(1)] = m.group(2).strip()
            continue
        m = DEFINE_FN.match(line)
        if m:
            body = m.group(3)
            cast = CAST.search(body)
            #
            # A cast-bearing function-like macro is an accessor whatever
            # it adds: `T41_RX(obj)` casts and adds, `HDX_R38(h)` only
            # indexes `r36[2]`.  Both express layout through casts, which
            # is what this census records.
            #
            if cast and ("+" in body or "[" in body):
                t = cast.group(1)
                w = SCALAR_W.get(t) if "*" not in cast.group(2) or t in SCALAR_W else 4
                if cast.group(2) == "**":
                    w = 4
                add = ADDEND.search(body)
                body_off = fold(add.group(1), defines) if add else None
                accessors[m.group(1)] = (w, body_off)
    return defines, accessors


def local_includes(path, seen=None):
    """Transitive local #include closure, for the per-file define table."""
    if seen is None:
        seen = set()
    if path in seen or not path.is_file():
        return seen
    seen.add(path)
    for line in path.read_text(errors="replace").splitlines():
        m = INCLUDE_LOCAL.match(line)
        if m:
            inc = m.group(1)
            for cand in (path.parent / inc, ROOT / "include" / "dsplib" / inc,
                         ROOT / "include" / inc, ROOT / "src" / inc):
                if cand.is_file():
                    local_includes(cand, seen)
                    break
    return seen


def function_regions(lines):
    """Best-effort (start_line, end_line, name, params) spans; braces only.

    The tree defines functions two ways -- return type on its own line with
    the name on the next, and everything on one line -- both at column 0.
    Attribution is a label for the report, not load-bearing for grouping,
    so the tracker counts braces and reports how much of each file it
    failed to attribute.  Parameter names come along because a helper's
    own body reads `+ off` through its parameter, and that dynamic
    site is exactly one of the shapes the issue records.
    """
    spans, stack, depth = [], [], 0
    # `unsigned char t3c_getb(` and the tree's two-line style `t3c_getb(`
    # both match: the optional prefix ends at a word boundary, so a bare
    # name line falls through to the name-only reading.
    sig = re.compile(r"^(?:[A-Za-z_][\w \t\*&]*\b)?([A-Za-z_]\w*)\s*\(")

    def params_of(idx):
        text = lines[idx]
        if "(" not in text:
            return {}
        #
        # A signature may span lines; join until its parens balance so
        # `VPcmV34InitMOH(void *objp, int message,` still yields its
        # parameter names.
        #
        j = idx
        while j + 1 < len(lines) and \
                text.count("(") > text.count(")") and j - idx < 4:
            j += 1
            text += " " + lines[j]
        inner = text[text.index("("):text.rindex(")") + 1]
        params = {}
        for arg in inner.strip("()").split(","):
            toks = re.findall(r"[A-Za-z_]\w*", arg)
            if not toks or toks[-1] in ("void", "const", "unsigned", "signed",
                                        "struct", "int", "short", "char"):
                continue
            params[toks[-1]] = arg.strip()
        return params

    def name_and_params(idx):
        """Resolve a function name walking back over a multi-line sig."""
        j, steps = idx, 0
        while j >= 0 and steps < 4:
            cand = lines[j]
            if j < idx and (";" in cand or "}" in cand or "{" in cand):
                break
            m = sig.match(cand)
            if m and "(" in cand:
                return m.group(1), j
            j -= 1
            steps += 1
        return None, idx

    for idx, line in enumerate(lines):
        opens = line.count("{") - line.count("}")
        if depth == 0 and opens > 0:
            # An initializer's brace (`char *MSG[10] = {`) opens a data
            # region, not a function; attribute it to no one.  The depth
            # still counts below -- exactly once.
            if re.search(r"=\s*\{\s*$", line) or \
                    line.lstrip().startswith(("typedef", "struct", "enum",
                                             "union")):
                pass
            else:
                name, sigidx = name_and_params(idx)
                stack.append((idx, name or "?", params_of(sigidx)))
        depth += opens
        if depth <= 0 and stack:
            start, name, params = stack.pop()
            spans.append((start, idx, name, params))
            depth = 0
        elif depth < 0:
            depth = 0
    return spans


def scan_file(path):
    """Every raw-offset access site in one file.  Returns (sites, stats)."""
    raw = path.read_text(errors="replace")
    text = strip_comments_strings(raw)
    lines = text.splitlines()

    # Define table: this file plus its local include closure.
    defines, accessors = {}, {}
    for inc in sorted(local_includes(path)):
        d, a = collect_defines(inc.read_text(errors="replace"))
        defines.update(d)
        accessors.update(a)

    spans = function_regions(lines)

    def region(idx):
        for start, end, name, _params in spans:
            if start <= idx <= end:
                return name
        return "(none)"

    def params_at(idx):
        for start, end, _name, params in spans:
            if start <= idx <= end:
                return params
        return {}

    def helper_width(name):
        acc = accessors.get(name)
        if acc is not None:
            return acc[0]
        for suf, w in sorted(HELPER_SUFFIX.items(), key=lambda kv: -len(kv[0])):
            if name.endswith(suf):
                return w
        return None

    def helper_body_off(name):
        acc = accessors.get(name)
        return acc[1] if acc is not None else None

    #
    # A line whose only `+` is its last character carries its addend on
    # the next line (`(char *)modem +\n\t\t\tV17RX_OBJ_STATE);`), so the
    # castadd pass scans the join and records it at the cast's line.
    #
    def scan_text(idx):
        s = lines[idx]
        if re.search(r"\+\s*$", s) and idx + 1 < len(lines):
            return s + " " + lines[idx + 1]
        return s

    def byteview_text(idx):
        s = scan_text(idx)
        # `(...)[\n OFF + k]` splits a byte view's index across lines.
        if not BYTEVIEW.search(s) and s.rstrip().endswith("[") \
                and idx + 1 < len(lines):
            return s + " " + lines[idx + 1]
        return s

    sites = []
    for idx, line in enumerate(lines):
        line = scan_text(idx)
        #
        # Attribute each `+ offset` to the OUTERMOST pointer cast that
        # precedes it.  `(struct v34_receiver *)((char *)obj + 0x264)` has
        # two casts; the inner one is arithmetic dressing, and recording it
        # would label the site `char *` and lose the recovered owner type.
        # A cast whose operand merely holds the base (`(char *)obj + 0x146c`)
        # still governs, so "precedes" is the only requirement; the guard
        # below keeps a cast on a distant argument from claiming an
        # unrelated addend.
        #
        for add in ADDEND.finditer(line):
            p = add.start()
            cands = [c for c in CAST.finditer(line) if c.end() <= p]
            if not cands:
                continue
            m = cands[0]
            between = line[m.end():p]
            if len(between) > 48 or re.search(r"[,;]", between):
                continue
            base = re.search(r"([A-Za-z_][\w\-\.>\[\]]*)\s*$", line[:p])
            if not base:
                continue
            ctype, stars = m.group(1), m.group(2)
            off = fold(add.group(1), defines)
            sites.append({
                "line": idx + 1, "shape": "castadd",
                "target": ctype + " " + stars,
                "width": SCALAR_W.get(ctype, 4) if stars == "*" else 4,
                "base": base_shape(base.group(1)),
                "offset": add.group(1).strip(),
                "resolved": off,
                "function": region(idx),
            })
        #
        # Parameter-led addends: a helper's own body reads `+ off` through
        # its offset parameter, which no constant-led ADDEND can match.
        # Recording it keeps the accessor definitions themselves in the
        # ledger -- the issue lists them -- without opening the floodgate
        # to every lowercase `+ var` in sample arithmetic.
        #
        for m in re.finditer(r"\+\s*([A-Za-z_]\w*)", line):
            ident = m.group(1)
            if ident not in params_at(idx):
                continue
            if ADDEND.match(line[m.start():]):
                continue
            p = m.start()
            cands = [c for c in CAST.finditer(line) if c.end() <= p]
            if not cands:
                continue
            cm = cands[0]
            between = line[cm.end():p]
            if len(between) > 48 or re.search(r"[,;]", between):
                continue
            base = re.search(r"([A-Za-z_][\w\-\.>\[\]]*)\s*$", line[:p])
            if not base:
                continue
            ctype, stars = cm.group(1), cm.group(2)
            sites.append({
                "line": idx + 1, "shape": "castadd",
                "target": ctype + " " + stars,
                "width": SCALAR_W.get(ctype, 4) if stars == "*" else 4,
                "base": base_shape(base.group(1)),
                "offset": m.group(1), "resolved": None,
                "function": region(idx),
            })
        if line.lstrip().startswith("#"):
            continue
        for m in HELPER_CALL.finditer(line):
            name, base, offexpr = m.groups()
            if name not in HELPER_NAMES and name not in accessors:
                continue
            if m.start() == 0:
                # A definition signature, not a call: the tree writes the
                # name at column 0 on its own line.
                continue
            w = helper_width(name)
            if offexpr:
                off, offlabel = fold(offexpr, defines), offexpr.strip()
            else:
                # A one-argument accessor macro: the offset lives in the
                # macro body, so the site records that.
                off, offlabel = helper_body_off(name), "(accessor macro)"
            sites.append({
                "line": idx + 1, "shape": "helper", "target": name,
                "width": w, "base": base_shape(base),
                "offset": offlabel, "resolved": off,
                "function": region(idx),
            })
        for m in BYTEVIEW.finditer(byteview_text(idx)):
            offexpr = m.group(1)
            before = line[:m.start()]
            # A byte view's base may be parenthesized
            # (`((unsigned char *)(void *)&tx->cfg)[F]`), so allow closing
            # parens after the identifier.
            base = re.search(r"([A-Za-z_][\w\-\.>\[\]]*)\s*\)*\s*$", before)
            if not base:
                continue
            bt = base.group(1)
            # A constant index is a byte view only through byte-typed
            # bases.  Four ways to know: a declaration anywhere in the
            # enclosing function so far, a parameter whose declared type
            # says `char *`, a byte-pointer cast earlier on the same line
            # (`((unsigned char *)&tx->cfg)[F]`), or a local assigned from
            # one (`rx = (unsigned char *)modem;`) -- which can sit many
            # lines above its use, so the search reaches to the function
            # start, not a fixed window.  Anything else is an ordinary
            # array subscript and not this census's business.
            for start, end, _n, _p in spans:
                if start <= idx <= end:
                    ctx = "\n".join(lines[start:idx + 1])
                    break
            else:
                ctx = "\n".join(lines[max(0, idx - 8):idx + 1])
            pdecl = params_at(idx).get(bt, "")
            declared = re.search(
                r"(?:unsigned\s+char|signed\s+char|\bchar)\s*\*+\s*\**\b" +
                re.escape(bt) + r"\b", ctx)
            if not declared and pdecl and re.search(r"char\s*\*+", pdecl):
                declared = True
            if not declared:
                declared = re.search(
                    r"\((?:const\s+|volatile\s+)*(?:unsigned\s+|signed\s+)?"
                    r"char\s*\*+\s*\)", before)
            if not declared:
                declared = re.search(
                    r"\b" + re.escape(bt) +
                    r"\s*=\s*\(?(?:const\s+)?(?:unsigned\s+|signed\s+)?"
                    r"char\s*\*+", ctx)
            if not declared:
                continue
            off = fold(offexpr, defines)
            sites.append({
                "line": idx + 1, "shape": "byteview", "target": "char *",
                "width": 1, "base": base_shape(bt),
                "offset": offexpr, "resolved": off,
                "function": region(idx),
            })

    stats = {
        "lines": len(lines),
        "functions": len(spans),
        "sites": len(sites),
    }
    return sites, stats


def all_files():
    out = []
    for d in SCAN_DIRS:
        if not d.is_dir():
            continue
        for ext in ("*.c", "*.cpp", "*.h", "*.hpp"):
            out.extend(sorted(d.rglob(ext)))
    return [p for p in out if p.is_file()]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--json", metavar="PATH", help="write the full ledger")
    ap.add_argument("--markdown", metavar="PATH", help="issue-ready table")
    ap.add_argument("--groups", action="store_true",
                    help="wave ledger by (target, offset, width)")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()

    sites_by_file, stats = {}, {}
    for path in all_files():
        try:
            sites, st = scan_file(path)
        except Exception as e:  # noqa: BLE001 - report, keep scanning
            print(f"  scan FAILED {path}: {e}", file=sys.stderr)
            continue
        rel = str(path.relative_to(ROOT))
        sites_by_file[rel] = sites
        stats[rel] = st

    total = sum(s["sites"] for s in stats.values())
    files = len(stats)
    print(f"offsetcensus: {files} files, "
          f"{sum(s['lines'] for s in stats.values())} lines, "
          f"{total} raw-offset sites")

    shapes = Counter(s["shape"] for sites in sites_by_file.values() for s in sites)
    for shape in ("castadd", "helper", "byteview"):
        print(f"  {shape:9s} {shapes.get(shape, 0):5d}")
    folded = sum(1 for sites in sites_by_file.values()
                 for s in sites if s["resolved"] is not None)
    print(f"  offsets folded: {folded}, dynamic: {total - folded}")

    top = Counter(s["function"] for sites in sites_by_file.values() for s in sites)
    print("  functions with most sites: "
          + ", ".join(f"{n}={c}" for n, c in top.most_common(8)))

    if args.groups:
        groups = Counter()
        for sites in sites_by_file.values():
            for s in sites:
                key = (s["shape"], s["target"], s["resolved"], s["width"])
                groups[key] += 1
        print(f"distinct (shape, target, offset, width) groups: {len(groups)}")
        for (shape, target, off, w), n in groups.most_common(40):
            print(f"  {n:4d}  {shape:9s} {target:28s} "
                  f"off={off if off is not None else 'dyn':>8} w={w}")

    if args.json:
        Path(args.json).write_text(json.dumps({
            "total_sites": total, "files": files,
            "by_file": {k: v for k, v in sites_by_file.items() if v},
        }, indent=1))
        print(f"ledger written: {args.json}")
    if args.markdown:
        write_markdown(args.markdown, sites_by_file, total)
        print(f"markdown written: {args.markdown}")
    return 0


def write_markdown(out_path, sites_by_file, total):
    rows = []
    for f, sites in sorted(sites_by_file.items()):
        for s in sites:
            rows.append((f, s))
    with open(out_path, "w") as fh:
        fh.write(f"Issue #260 raw-offset census: {total} sites\n\n")
        fh.write("| File | Line | Function | Shape | Target | Base | Offset | Resolved |\n")
        fh.write("| --- | --- | --- | --- | --- | --- | --- | --- |\n")
        for f, s in rows:
            off = hex(s["resolved"]) if isinstance(s["resolved"], int) else "dynamic"
            fh.write(f"| {f} | {s['line']} | {s['function']} | {s['shape']} "
                     f"| {s['target']} | {s['base'][1]} | {s['offset']} | {off} |\n")


def self_test():
    """The F134 rule: prove the detector fires before trusting a clean run.

    The fixture is a cut-down translation unit carrying one of each shape
    with known answers, including the two ways a scanner goes wrong: a
    helper call whose offset is a loop variable, and a constant array
    subscript that is NOT a byte view.
    """
    fixture = r"""
#define T3C_FABF8	0xabf8
#define MP_RATEMASK	0x06
#define CFG_MIN_LEVEL	0x10
#define T41_RX(obj)	((struct v34_receiver *)((char *)(obj) + 0x0264))

struct v34_object { int x; };

static unsigned char
t3c_getb(const struct v34_object *obj, unsigned off)
{
	return *((const unsigned char *)obj + off);
}

void rxtiminginit(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	struct v34_receiver *rx2 = T41_RX(obj);
	int level = *(const int *)(obj + CFG_MIN_LEVEL);
	short v = t3c_getb(obj, T3C_FABF8);
	for (int i = 0; i < 4; i++)
		obj->scratch[i] = t3c_getb(obj, 0xaae6 + 2 * i);
}

unsigned char *cfg_tbl;
int lookup(int k) { return cfg_tbl[k]; }
"""
    d = Path("/tmp/opencode/offsetcensus_fixture.c")
    d.parent.mkdir(parents=True, exist_ok=True)
    d.write_text(fixture)
    sites, st = scan_file(d)
    fails = []

    def check(cond, what):
        if not cond:
            fails.append(what)

    check(st["sites"] > 0, "found no sites at all")
    castadds = [s for s in sites if s["shape"] == "castadd"]
    helpers = [s for s in sites if s["shape"] == "helper"]
    views = [s for s in sites if s["shape"] == "byteview"]
    check(len(castadds) == 3, f"castadd count {len(castadds)} != 3: {castadds}")
    check(len(helpers) == 3, f"helper count {len(helpers)} != 3: {helpers}")
    dyn_h = [s for s in helpers if s["resolved"] is None]
    check(len(dyn_h) == 1, f"expected exactly 1 dynamic helper offset, got {dyn_h}")
    acc = [s for s in helpers if s["offset"] == "(accessor macro)"]
    check(len(acc) == 1 and acc[0]["resolved"] == 0x264,
          f"missing the T41_RX accessor-macro site: {acc}")
    check(any(s["resolved"] is None and s["offset"] == "off"
              for s in castadds), "missing the helper-body `+ off` site")
    byoff = {s["resolved"]: s for s in castadds if s["resolved"] is not None}
    check(0x264 in byoff, "missing the +0x264 receiver cast")
    check(byoff.get(0x264, {}).get("target") == "struct v34_receiver *",
          f"+0x264 target wrong: {byoff.get(0x264)}")
    check(0x10 in byoff, "missing CFG_MIN_LEVEL fold")
    check(len(views) == 0, f"byteview fired on a plain array subscript: {views}")

    if fails:
        for f in fails:
            print(f"SELF-TEST FAIL: {f}")
        return 1
    print(f"self-test OK: {st['sites']} sites, all shape counts and folds correct")
    return 0


if __name__ == "__main__":
    sys.exit(main())
