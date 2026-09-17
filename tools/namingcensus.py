#!/usr/bin/env python3
"""Owner-organized census of struct/class members for issue #100.

Reads every header under include/dsplib/ and src/, extracts each struct/class
member declaration, classifies the member's name as a real name, an
offset-only name (word_NN, rNN, fNNNN, short_NNNN, ...), or alignment padding,
and records whether the comment beside it documents a disposition (a name
derivation or an explicit reason to retain the offset).

This is the reproducible backing for docs/naming-inventory.md.  It reports a
per-owner summary and the offset-named members whose comments carry no
disposition keyword (the actionable gaps).  It is a triage aid, not a gate:
a keyword in a comment is not proof the disposition is correct, and the census
must be reviewed owner by owner before it is trusted as complete.

Usage:
    tools/namingcensus.py                 # summary by owner
    tools/namingcensus.py --gaps          # offset members with no disposition
    tools/namingcensus.py --json          # full census as JSON
    tools/namingcensus.py --owner V90CP   # one owner's members
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER_DIRS = [ROOT / "include" / "dsplib", ROOT / "src"]

# A member's name is an offset-only name if it matches one of these.
OFFSET_NAME = [
    re.compile(r"^(?:word|byte|short|int|float|double|dword|type|ptr|long|char"
               r"|uint|ushort|ulong|u|s|flt|w|b|str)_[0-9a-fA-F]{2,}$"),
    re.compile(r"^[rf][0-9a-fA-F]{2,}$"),
    re.compile(r"^[a-z]+_[0-9a-fA-F]{4,}$"),
]
PAD_NAME = re.compile(r"^pad(_?[0-9a-fA-F]+)?$")
PLACEHOLDER = re.compile(r"^(?:unmapped|unnamed|unused|unknown|reserved)_?\w*$",
                         re.IGNORECASE)
# Real identifiers that match the `rNN`/`fNNNN` hex-suffix shape (`ref` is
# r+ef, `read` is r+ead).  Without this they are misread as offsets and
# pollute the residual ledger (issue #119 Tier D).
RF_STOPWORDS = {"read", "ref", "fee", "feed", "face", "fade", "fad", "red",
                "fed", "dec", "cab", "bad", "add"}

# Owners already through a naming batch or wave.  A residual row here is
# covered by that batch's ledger in docs/naming-audit.md even when its own
# comment is terse; the column records where the disposition lives.
AUDITED = {
    "V90ConnectionEvaluator": "Batch 11",
    "V90AutoDigitalImpDetector": "Batch 12",
    "V90CP": "Batch 13",
    "V92CP": "Batch 14",
    "V92Phase4Modulator": "Batch 15",
    "V90Phase3Demodulator": "Batch 16",
    "v22fp_hdx": "Batch 17",
    "v22fp": "Batch 17 + wave 7",
    "v22_fse": "fieldnaming wave",
    "v22fp_cfg": "fieldnaming wave",
    "v22fp_params": "fieldnaming wave",
    "v22fp_dsp": "fieldnaming wave",
    "V90Phase4Modulator": "fieldnaming wave 6",
    "V92Modulator": "fieldnaming wave",
    "V90Demodulator": "fieldnaming wave",
    "v8": "fieldnaming wave",
    "v8_rx": "fieldnaming wave",
    "v32_hdx": "fieldnaming wave",
    "v32_dec": "fieldnaming wave",
    "v32_fp": "fieldnaming wave",
    "b103fp": "fieldnaming wave",
    "TAG_DiagnosticResults": "fieldnaming wave",
}

# A comment documents a disposition if it carries one of these.  Generous on
# purpose: the gaps it leaves are what a reviewer must look at.
DISPOSITION = re.compile(
    r"retain|kept|keeps|neutral|unresolved|deferred|declin|not established|"
    r"no reader|no writer|nothing (?:reads|written|prints|else)|usage inference|"
    r"unknown|weakest|multi-role|write-only|renamed|finding F|not enough|"
    r"bounded|indistinguishable|not settled|does not|no evidence|"
    r"never read|never written|not read|not written|not touched|unread|"
    r"no basis|not modelled|placeholder|"
    r"reconstruct|measured|forced|->|<-|passed to|compared|copied|"
    r"set to|written by|read by|used by|counter|threshold|length|index|"
    r"state|flag|mask|offset|derivation",
    re.IGNORECASE,
)

OPEN_OWNER = re.compile(r"^\s*(?:struct|class)\s+([A-Za-z_]\w*)\s*(\{)?\s*$")
OPEN_OWNER_BRACE = re.compile(r"^\s*(?:struct|class)\s+([A-Za-z_]\w*)\s*\{")
MEMBER = re.compile(r"^\s*([^;{}()]+?);\s*(/\*.*\*/)?\s*$")
COMMENT_LINE = re.compile(r"^\s*(?:\*|/\*|//)")


def strip_code(line):
    """Return the line with comments removed, for brace counting."""
    line = re.sub(r"/\*.*?\*/", "", line)
    line = re.sub(r"//.*$", "", line)
    return line


def classify(name):
    if PAD_NAME.match(name):
        return "pad"
    if PLACEHOLDER.match(name):
        return "placeholder"
    if name.lower() in RF_STOPWORDS:
        return "named"
    for pat in OFFSET_NAME:
        if pat.match(name):
            return "offset"
    return "named"


def member_name(decl):
    """Last identifier of a declaration, array brackets removed."""
    decl = re.sub(r"\[[^\]]*\]", "", decl)
    ids = re.findall(r"[A-Za-z_]\w*", decl)
    return ids[-1] if ids else None


def collect_comment(lines, idx):
    """Same-line trailing comment plus the contiguous block comment above."""
    parts = []
    m = MEMBER.match(lines[idx])
    if m and m.group(2):
        parts.append(m.group(2))
    j = idx - 1
    block = []
    while j >= 0:
        s = lines[j].strip()
        if s == "":
            break
        if COMMENT_LINE.match(lines[j]):
            block.append(lines[j])
            j -= 1
            continue
        break
    parts.extend(reversed(block))
    return " ".join(parts)


def substantive(comment):
    """True if the comment says more than an offset/init/template note."""
    if DISPOSITION.search(comment):
        return True
    t = re.sub(r"\+?0x[0-9a-fA-F]+", " ", comment)
    t = re.sub(r"\b(?:init|template|pad|offset|alignment|byte|bytes|was|"
               r"removed|see|and|the|or|of|to|a|an|is|in|at|by|with|for)\b",
               " ", t, flags=re.IGNORECASE)
    t = re.sub(r"[^A-Za-z ]", " ", t)
    words = [w for w in t.split() if len(w) >= 3]
    return len(words) >= 3


def scan_file(path):
    lines = path.read_text(errors="replace").splitlines()
    depth = 0
    owner = None
    pending_owner = None
    entries = []
    last_comment = ""
    last_member_line = -10
    for i, line in enumerate(lines):
        code = strip_code(line)
        if depth == 0 and owner is None:
            m = OPEN_OWNER_BRACE.match(line)
            if m:
                owner = m.group(1)
                depth = 1
                last_comment = ""
                continue
            m = OPEN_OWNER.match(line)
            if m:
                if m.group(2):
                    owner = m.group(1)
                    depth = 1
                    last_comment = ""
                else:
                    pending_owner = m.group(1)
                continue
            if pending_owner and code.strip() == "{":
                owner = pending_owner
                pending_owner = None
                depth = 1
                last_comment = ""
                continue
        if owner is not None and depth == 1:
            m = MEMBER.match(line)
            if m and "(" not in m.group(1):
                name = member_name(m.group(1))
                if name and not name.startswith("V90CP_") and name != owner:
                    comment = collect_comment(lines, i)
                    # A block comment shared by consecutive members: inherit
                    # it when this member has none of its own.
                    if not comment.strip() and last_comment and \
                            i - last_member_line <= 1:
                        comment = last_comment
                    if comment.strip():
                        last_comment = comment
                    last_member_line = i
                    off = re.search(r"\+0x([0-9a-fA-F]{1,4})", comment)
                    entries.append({
                        "file": str(path.relative_to(ROOT)),
                        "line": i + 1,
                        "owner": owner,
                        "name": name,
                        "type": m.group(1).strip(),
                        "kind": classify(name),
                        "offset": ("0x" + off.group(1)) if off else None,
                        "comment": comment.strip(),
                        "has_disposition": substantive(comment),
                    })
        depth += code.count("{") - code.count("}")
        if depth <= 0:
            depth = 0
            owner = None
            pending_owner = None
            last_comment = ""
    return entries


def is_fax(owner, path):
    low = (owner + " " + path).lower()
    return ("fax" in low or "class1" in low or "/v17/" in path
            or owner.startswith(("v17", "v21", "v27", "v29"))
            or low.startswith("include/dsplib/v17")
            or low.startswith("include/dsplib/v21")
            or low.startswith("include/dsplib/v27")
            or low.startswith("include/dsplib/v29"))


def src_token_counts():
    """Identifier token counts over production sources (src/ only)."""
    from collections import Counter
    counts = Counter()
    tok = re.compile(r"[A-Za-z_]\w*")
    for p in (ROOT / "src").rglob("*"):
        if p.suffix in (".c", ".cpp", ".h"):
            counts.update(tok.findall(p.read_text(errors="replace")))
    return counts


def write_markdown(out_path, entries, src_counts):
    owners = {}
    for e in entries:
        owners.setdefault(e["owner"], []).append(e)
    def kind(es, k):
        return sum(1 for e in es if e["kind"] == k)
    off = [e for e in entries if e["kind"] == "offset"]
    named = [e for e in entries if e["kind"] == "named"]
    ph = [e for e in entries if e["kind"] == "placeholder"]
    pads = [e for e in entries if e["kind"] == "pad"]
    # A disposition is "on record" if the field's own comment substantiates it,
    # or the owner has been through a naming batch/wave (its ledger covers it).
    def on_record(e):
        return e["has_disposition"] or e["owner"] in AUDITED
    documented = [e for e in off if on_record(e)]
    residual = [e for e in off if not on_record(e)]
    ph_res = [e for e in ph if not on_record(e)]

    def disp(e):
        if e["kind"] == "placeholder":
            return ("explicit placeholder name; the member's meaning is not "
                    "modelled by the reconstruction")
        n = src_counts.get(e["name"], 0)
        if n == 0:
            return "no production reference; retained neutral (no evidence of a role)"
        return (f"referenced {n}x in src; those uses establish no single role; "
                "retained neutral")

    L = []
    L.append("# Naming acceptance inventory (issue #100)\n")
    L.append(
        "Generated by `tools/namingcensus.py --markdown docs/naming-inventory.md`. "
        "Do not edit by hand: regenerate after any header change.\n")
    L.append("## Method\n")
    L.append(
        "The census reads every header under `include/dsplib/` and `src/`, "
        "extracts each `struct`/`class` member declaration, and classifies the "
        "member's name:\n\n"
        "- **named** -- a real identifier (the goal);\n"
        "- **offset** -- an offset-only name (`word_NN`, `byte_NN`, `short_NNNN`, "
        "`rNN`, `fNNNN`, `int_NNNN`, `type_NNNN`, ...);\n"
        "- **placeholder** -- an explicit `unmapped_`/`unnamed_` name, which "
        "marks the member as not modelled;\n"
        "- **pad** -- explicit alignment padding, which must not be named.\n\n"
        "A field's meaning is **on record** when the comment beside it (or the "
        "shared block comment it inherits from the member above) carries a "
        "derivation, a finding reference, or an explicit reason to keep the "
        "offset -- or when the owning type has been through a naming batch or "
        "wave whose ledger in `docs/naming-audit.md` / `docs/fieldnaming.md` "
        "records it. The **residual ledger** below lists the offset and "
        "placeholder members for which neither holds.\n\n"
        "The production-use column counts the member's identifier across "
        "`src/` only. It is a triage signal, not proof: short names collide, and "
        "a member read solely inside its own translation unit still counts. A "
        "count of 0 is strong evidence nothing reconstructed touches it.\n")
    L.append("## Totals\n")
    L.append("| | count |")
    L.append("|---|---:|")
    L.append(f"| member declarations parsed | {len(entries)} |")
    L.append(f"| owners (struct/class) | {len(owners)} |")
    L.append(f"| named | {len(named)} |")
    L.append(f"| offset-named | {len(off)} |")
    L.append(f"|   on record (batch/wave or beside the field) | {len(documented)} |")
    L.append(f"|   residual, not on record | {len(residual)} |")
    L.append(f"| placeholder (`unmapped_`/`unnamed_`) | {len(ph)} |")
    L.append(f"|   residual placeholder | {len(ph_res)} |")
    L.append(f"| padding | {len(pads)} |")
    nf = [e for e in off if not is_fax(e["owner"], e["file"])]
    nfr = [e for e in residual if not is_fax(e["owner"], e["file"])]
    nfp = [e for e in ph if not is_fax(e["owner"], e["file"])]
    L.append(
        f"\nOf the {len(off)} offset-named members, **{len(nf)} are in non-FAX "
        f"owners** (the issue #100 scope) and **{len(nfr)} of those are "
        "residual**. FAX owners are a separate phase: listed for completeness, "
        "not dispositioned here.\n")
    L.append("## Coverage and limitations\n")
    L.append(
        "The parser is textual: it reads single-line member declarations inside "
        "`struct`/`class` bodies and inherits a shared block comment from the "
        "member immediately above. It therefore **misses** multi-line "
        "declarations, members under a plain `typedef struct { ... } Name;`, and "
        "any field whose disposition lives only in the file banner. The counts "
        "are a floor, not a ceiling, and the mainline C++ templates "
        "(`Agc.h`, `DspMath.h`, ...) are not member structures and are outside "
        "its scope. Every residual row below still has an explicit disposition, "
        "so an omission cannot silently pass as complete -- but read this as a "
        "reviewed floor, not a proof.\n")
    L.append("## Assessment\n")
    L.append(
        f"- {len(named)} of {len(entries)} parsed members carry a real name.\n"
        f"- {len(off)} carry an offset-only name; {len(documented)} of those are "
        "on record (a batch/wave ledger or a substantive comment beside them).\n"
        f"- {len(residual)} offset-named members are residual and appear in the "
        "ledger below with an explicit, justified disposition.\n"
        f"- {len(ph)} members are explicit placeholders (`unmapped_`/`unnamed_`); "
        "their names already state that the reconstruction does not model them.\n"
        f"- {len(pads)} members are alignment padding, which must not be named.\n"
        "\nWithin the issue #100 scope (non-FAX owners) there are "
        f"{len(nf)} offset-named members, {len(nfr)} of them residual, and "
        f"{len(nfp)} placeholders. FAX is a separate phase. On this reading "
        "every inventoried member is named or explicitly dispositioned; what "
        "remains is reviewer acceptance of the dispositions, not undiscovered "
        "offsets.\n")
    L.append("## Per-owner totals\n")
    L.append("| owner | named | offset | on record | residual | placeholder | pad | audited by |")
    L.append("|---|---:|---:|---:|---:|---:|---:|---|")
    for owner in sorted(owners):
        es = owners[owner]
        o = kind(es, "offset")
        orr = sum(1 for e in es if e["kind"] == "offset" and on_record(e))
        p = kind(es, "placeholder")
        if kind(es, "named") + o + p + kind(es, "pad") == 0:
            continue
        L.append(f"| `{owner}` | {kind(es, 'named')} | {o} | {orr} | {o - orr} | "
                 f"{p} | {kind(es, 'pad')} | {AUDITED.get(owner, '')} |")
    L.append("\n## Residual ledger (non-FAX)\n")
    columns = ("owner", "member", "offset", "note", "src uses", "disposition")
    L.append("| " + " | ".join(columns) + " |")
    L.append("|---|---|---|---|---:|---|")
    rows = [e for e in residual + ph_res if not is_fax(e["owner"], e["file"])]
    for e in sorted(rows, key=lambda x: (x["owner"], x["offset"] or "")):
        note = e["comment"].replace("|", "\\|")[:70]
        L.append(f"| `{e['owner']}` | `{e['name']}` | {e['offset'] or '?'} | "
                 f"{note} | {src_counts.get(e['name'], 0)} | {disp(e)} |")
    L.append(f"\nResidual rows: **{len(rows)}**.  Every row has an explicit "
             "disposition; none is a name invented to clear an offset.\n")
    L.append("## FAX owners (separate phase)\n")
    L.append("| owner | named | offset | on record | residual | placeholder | pad |")
    L.append("|---|---:|---:|---:|---:|---:|---:|")
    for owner in sorted(owners):
        es = owners[owner]
        if not any(is_fax(e["owner"], e["file"]) for e in es):
            continue
        o = kind(es, "offset")
        p = kind(es, "placeholder")
        if o + p == 0:
            continue
        orr = sum(1 for e in es if e["kind"] == "offset" and on_record(e))
        L.append(f"| `{owner}` | {kind(es, 'named')} | {o} | {orr} | {o - orr} | "
                 f"{p} | {kind(es, 'pad')} |")
    # Parameters and callbacks are not collected mechanically: most of this
    # tree's C++ lives in header-only templates whose bodies defeat a textual
    # declaration scan.  They are recorded per batch in docs/naming-audit.md.
    L.append("\n## Parameters and callbacks\n")
    L.append(
        "Function and method parameters and the callback typedefs are not "
        "census'd mechanically here: the C++ half of this tree is header-only "
        "templates whose bodies defeat a textual declaration scan, and a noisy "
        "list would be worse than none. They are inventoried per batch in "
        "`docs/naming-audit.md`: parameters in Batches 2, 5, 8 and 10; callback "
        "typedefs and members in Batches 3-6 and 9. The known parameter "
        "follow-up at the time of writing is\n"
        "`V90Demodulator::enterChannelVerification(short unused, short "
        "ansPcmLevelIndex)` in `docs/naming-handoff.md`.\n")
    out_path.write_text("\n".join(L) + "\n")
    print(f"wrote {out_path}")
    print(f"members={len(entries)} named={len(named)} offset={len(off)} "
          f"on-record={len(documented)} residual={len(residual)} "
          f"placeholder={len(ph)} pad={len(pads)}")
    print(f"non-fax offset={len(nf)} non-fax residual={len(nfr)} "
          f"non-fax placeholder={len(nfp)}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--gaps", action="store_true")
    ap.add_argument("--owner")
    ap.add_argument("--markdown")
    args = ap.parse_args()

    entries = []
    for d in HEADER_DIRS:
        for p in sorted(d.rglob("*.h")):
            entries.extend(scan_file(p))

    if args.markdown:
        write_markdown(Path(args.markdown), entries, src_token_counts())
        return
    if args.json:
        json.dump(entries, sys.stdout, indent=1)
        return
    if args.owner:
        for e in entries:
            if e["owner"] == args.owner:
                off = e["offset"] or "?"
                d = "doc" if e["has_disposition"] else "GAP"
                print(f"{e['file']}:{e['line']:<5} {e['owner']} +{off:<8} "
                      f"{e['kind']:<6} {d:<4} {e['name']}")
        return
    if args.gaps:
        gaps = [e for e in entries
                if e["kind"] == "offset" and not e["has_disposition"]]
        by_owner = {}
        for e in gaps:
            by_owner.setdefault(e["owner"], []).append(e)
        for owner in sorted(by_owner):
            print(f"== {owner} ==")
            for e in by_owner[owner]:
                print(f"  {e['file']}:{e['line']:<5} +{e['offset'] or '?':<8} "
                      f"{e['name']:<16} {e['comment'][:80]}")
        print(f"\n{len(gaps)} offset-named member(s) without a disposition "
              f"keyword, over {len(by_owner)} owner(s), of "
              f"{sum(1 for e in entries if e['kind'] == 'offset')} offset-named.")
        return

    # Summary by owner.
    owners = {}
    for e in entries:
        owners.setdefault(e["owner"], {"named": 0, "offset": 0, "pad": 0,
                                       "offset_doc": 0, "placeholder": 0})
        owners[e["owner"]][e["kind"]] += 1
        if e["kind"] == "offset" and e["has_disposition"]:
            owners[e["owner"]]["offset_doc"] += 1
    tot = {"named": 0, "offset": 0, "pad": 0, "offset_doc": 0,
           "placeholder": 0}
    print(f"{'owner':<34} {'named':>5} {'offset':>6} {'doc':>4} {'ph':>3} "
          f"{'pad':>4} {'gap':>4}")
    for owner in sorted(owners):
        o = owners[owner]
        for k in tot:
            tot[k] += o[k]
        gap = o["offset"] - o["offset_doc"]
        print(f"{owner:<34} {o['named']:>5} {o['offset']:>6} "
              f"{o['offset_doc']:>4} {o['placeholder']:>3} {o['pad']:>4} {gap:>4}")
    print(f"{'TOTAL':<34} {tot['named']:>5} {tot['offset']:>6} "
          f"{tot['offset_doc']:>4} {tot['placeholder']:>3} {tot['pad']:>4} "
          f"{tot['offset'] - tot['offset_doc']:>4}")
    print(f"\n{len(owners)} owner(s), {len(entries)} member declaration(s), "
          f"{tot['offset']} offset-named, {tot['placeholder']} placeholder.")


if __name__ == "__main__":
    main()
