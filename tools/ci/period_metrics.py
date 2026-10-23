#!/usr/bin/env python3
"""Render and check a base-versus-head Gentoo period metric comparison."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


METRICS = ("exact", "regalloc", "unresolved", "reloc", "bytes", "size",
           "nodata", "compared", "reference_functions", "exact_bytes",
           "compared_bytes")
MARKER = "<!-- gentoo-period-metrics -->"
ROOT = Path(__file__).resolve().parents[2]


def body_digest(raw, relocs):
    """Pin actual body bytes and canonical relocation kinds/destinations."""
    if not raw or relocs is None:
        raise ValueError("function has no measured body")
    # repr retains bytes/tuple type distinctions in canonical relocation keys.
    payload = ["period-body-v1", raw.hex(),
               [[offset, kind, repr(target)]
                for offset, (kind, target) in sorted(relocs.items())]]
    encoded = json.dumps(payload, ensure_ascii=True, separators=(",", ":"))
    return hashlib.sha256(encoded.encode("ascii")).hexdigest()


def function_digest(objects, symbol):
    """Refuse absent or ambiguous definitions; never select a lucky copy."""
    sys.path.insert(0, str(ROOT / "tools/toolchain"))
    import byteident
    matches = [path for path in sorted(Path(objects).glob("*.o"))
               if symbol in byteident.sizes(str(path))]
    if len(matches) != 1:
        raise ValueError("%s has %d defining objects; exactly one is required" %
                         (symbol, len(matches)))
    raw, relocs = byteident.body(str(matches[0]), symbol)
    return body_digest(raw, relocs)


def load_reviews(path):
    try:
        with open(path) as source:
            data = json.load(source)
    except (OSError, ValueError) as exc:
        raise ValueError("cannot read review record: %s" % exc)
    if not isinstance(data, dict) or data.get("version") != 1 or not isinstance(data.get("reviews"), list):
        raise ValueError("review record requires version 1 and a reviews list")
    seen = set()
    for review in data["reviews"]:
        if not isinstance(review, dict):
            raise ValueError("each reviewed loss must be an object")
        for name, width in (("base_sha", 40), ("accepted_commit", 40),
                            ("evidence_commit", 40),
                            ("head_body_sha256", 64)):
            if not re.fullmatch("[0-9a-f]{%d}" % width, str(review.get(name, ""))):
                raise ValueError("review requires full %s" % name)
        if not isinstance(review.get("symbol"), str) or not review["symbol"]:
            raise ValueError("review requires a symbol")
        evidence = review.get("evidence")
        if not isinstance(evidence, str) or not evidence:
            raise ValueError("review requires an evidence path")
        evidence_path = ROOT / evidence.split("#", 1)[0]
        if (not evidence_path.resolve().is_relative_to(ROOT) or
                not evidence_path.is_file()):
            raise ValueError("review evidence must exist inside the repository")
        key = (review["base_sha"], review["symbol"])
        if key in seen:
            raise ValueError("duplicate reviewed loss for %s" % review["symbol"])
        seen.add(key)
    return data["reviews"]


def review_losses(lost, reviews, base_sha, digest):
    """Return matched reviews and rejection reasons, scoped to actual losses."""
    accepted, rejected = {}, {}
    for symbol in lost:
        candidates = [item for item in reviews if item["symbol"] == symbol]
        matched = [item for item in candidates if item["base_sha"] == base_sha]
        if not matched:
            rejected[symbol] = "stale base review" if candidates else "no review"
            continue
        review = matched[0]
        try:
            actual = digest(symbol)
        except ValueError as exc:
            rejected[symbol] = str(exc)
            continue
        if actual != review["head_body_sha256"]:
            rejected[symbol] = "changed function bytes or relocation identity"
            continue
        accepted[symbol] = review
    return accepted, rejected


def _byteident():
    """The ledger schema has ONE owner: tools/toolchain/byteident.py."""
    sys.path.insert(0, str(ROOT / "tools/toolchain"))
    import byteident
    return byteident


def validate_displacements(data):
    """Validate tools/ci/displaced.json with byteident's own schema rules."""
    return _byteident().validate_displaced(data, root=str(ROOT))


def load_displacements(path):
    try:
        with open(path) as source:
            data = json.load(source)
    except (OSError, ValueError) as exc:
        raise ValueError("cannot read displacement ledger: %s" % exc)
    return validate_displacements(data)


def displacement_losses(lost, displaced, digest, root=ROOT):
    """Accepted exact-loss displacements, and why the rest are rejected.

    A SELECTED-only entry never reaches this function's acceptance path: an
    exact loss is about the worst per-TU body, so only `metric=exact|both`
    may excuse it.  `metric=both` is accepted only when its `body_sha256`
    matches the same head-object digest the reviewed-loss path uses.
    """
    accepted, rejected = {}, {}
    ledger = _byteident()
    for symbol in lost:
        entries = [entry for entry in displaced
                   if isinstance(entry, dict)
                   and entry.get("symbol") == symbol]
        exact = [entry for entry in entries
                 if entry.get("metric") in ("exact", "both")]
        if not exact:
            selected_only = any(entry.get("metric") == "selected"
                                for entry in entries)
            rejected[symbol] = (
                "selected-only displacement does not excuse an EXACT loss"
                if selected_only else "no reviewed displacement")
            continue
        if len(exact) > 1:
            rejected[symbol] = "ambiguous displacement entries"
            continue
        entry = exact[0]
        problem = ledger.ledger_entry_problem(entry, root=root)
        if problem is not None:
            rejected[symbol] = problem
            continue
        try:
            actual = digest(symbol)
        except ValueError as exc:
            rejected[symbol] = str(exc)
            continue
        if actual != entry.get("body_sha256"):
            rejected[symbol] = "displacement body hash does not match head"
            continue
        accepted[symbol] = entry
    return accepted, rejected


def load(path):
    try:
        with open(path) as f:
            metrics = json.load(f)
    except (OSError, ValueError) as exc:
        raise ValueError("cannot read %s: %s" % (path, exc))

    missing = [name for name in METRICS
               if not isinstance(metrics.get(name), int) or metrics[name] < 0]
    if missing:
        raise ValueError("%s lacks non-negative integer metric(s): %s" %
                         (path, ", ".join(missing)))
    symbols = metrics.get("exact_symbols")
    if (not isinstance(symbols, list) or
            any(not isinstance(symbol, str) for symbol in symbols) or
            len(set(symbols)) != len(symbols) or len(symbols) != metrics["exact"]):
        raise ValueError("%s has an invalid exact-symbol set" % path)
    if metrics["reference_functions"] == 0:
        raise ValueError("%s has no reference functions" % path)
    if metrics["compared"] > metrics["reference_functions"]:
        raise ValueError("%s compares more functions than the reference contains" % path)
    if metrics["compared_bytes"] == 0:
        raise ValueError("%s has no compared reference bytes" % path)
    if metrics["exact_bytes"] > metrics["compared_bytes"]:
        raise ValueError("%s has more exact bytes than compared reference bytes" %
                         path)
    buckets = ("exact", "regalloc", "unresolved", "reloc", "bytes", "size",
               "nodata")
    if sum(metrics[name] for name in buckets) != metrics["compared"]:
        raise ValueError("%s has a bucket total different from functions compared" %
                         path)
    return metrics


def delta(value):
    return "%+d" % value


def status_cell(change, better):
    """Directional status for an informational row.

    `better` is the sign of change that improves the metric: +1 when more is
    better, -1 when less is better. A zero change is neither, and is reported
    as such rather than borrowed into one direction.
    """
    if change == 0:
        return ":white_circle: no change"
    improved = change > 0 if better > 0 else change < 0
    return ":green_circle: better" if improved else ":red_circle: worse"


def coverage(metrics):
    return "%d / %d (%.1f%%)" % (metrics["compared"],
                                  metrics["reference_functions"],
                                  100.0 * metrics["compared"] /
                                  metrics["reference_functions"])


def byte_coverage(metrics):
    return "%d / %d (%.1f%%)" % (metrics["exact_bytes"],
                                  metrics["compared_bytes"],
                                  100.0 * metrics["exact_bytes"] /
                                  metrics["compared_bytes"])


def render(base, head, reviewed=None, rejected=None, displaced=None):
    """Render the base-versus-head markdown table and return (lines, lost).

    `lost` always includes every loss, including explicitly reviewed losses
    and reviewed displacements.  The caller rejects any loss without matching
    base/body review evidence or a hash-pinned displacement.  The two
    acceptance paths are reported separately: a displacement is a bounded
    layout/TU decision, not a claim that the old body was reviewed.
    """
    exact_delta = head["exact"] - base["exact"]
    lost = sorted(set(base["exact_symbols"]) - set(head["exact_symbols"]))
    gained = sorted(set(head["exact_symbols"]) - set(base["exact_symbols"]))
    exact_status = ("**:red_circle: regression**" if lost else
                    "**:green_circle: pass**")

    lines = [MARKER, "## Reconstruction Metrics (GCC 3.4.2 Gentoo)", "",
             "| Metric | Base | Head | Delta | Status |",
             "| --- | ---: | ---: | ---: | --- |"]
    lines.append("| Byte-identical functions | %d | %d | %s | %s |" %
                 (base["exact"], head["exact"], delta(exact_delta),
                  exact_status))
    exact_byte_delta = head["exact_bytes"] - base["exact_bytes"]
    lines.append("| Byte-identical reference code | %s | %s | %s bytes | %s |" %
                 (byte_coverage(base), byte_coverage(head),
                  delta(exact_byte_delta), status_cell(exact_byte_delta, 1)))
    compared_delta = head["compared"] - base["compared"]
    lines.append("| Functions compared | %s | %s | %s | %s |" %
                 (coverage(base), coverage(head), delta(compared_delta),
                  status_cell(compared_delta, 1)))
    not_yet_delta = ((head["compared"] - head["exact"]) -
                     (base["compared"] - base["exact"]))
    lines.append("| Not yet byte-identical | %d | %d | %s | %s |" %
                 (base["compared"] - base["exact"],
                  head["compared"] - head["exact"],
                  delta(not_yet_delta), status_cell(not_yet_delta, -1)))
    for name, label in (("regalloc", "Same code, different register allocation"),
                        ("reloc", "Bytes match, but call/data target differs"),
                        ("bytes", "Different bytes, same size"),
                        ("size", "Different size"),
                        ("unresolved", "Bytes match, target cannot be checked"),
                        ("nodata", "Could not be disassembled")):
        change = head[name] - base[name]
        lines.append("| %s | %d | %d | %s | %s |" %
                     (label, base[name], head[name], delta(change),
                      status_cell(change, -1)))
    lines.append("")
    if lost:
        lines.append("**Lost byte-identical functions:** " +
                     ", ".join("`%s`" % symbol for symbol in lost))
    if gained:
        lines.append("**New byte-identical functions:** " +
                     ", ".join("`%s`" % symbol for symbol in gained))
    if reviewed is not None:
        if displaced is None:
            lines.append("**Loss review:** %d reviewed, %d unreviewed / %d lost functions." %
                         (len(reviewed), len(lost) - len(reviewed), len(lost)))
        else:
            lines.append("**Loss review:** %d reviewed, %d displaced, %d "
                         "unreviewed / %d lost functions." %
                         (len(reviewed), len(displaced),
                          len(lost) - len(reviewed) - len(displaced),
                          len(lost)))
        for symbol, review in sorted(reviewed.items()):
            lines.append("- Reviewed loss `%s`: base `%s`, body SHA256 `%s`; "
                         "[evidence](%s), accepted source `%s`." %
                         (symbol, review["base_sha"], review["head_body_sha256"],
                          "https://github.com/philpem/slmodem_dsp_re/blob/" +
                          review["evidence_commit"] + "/" + review["evidence"],
                          review["accepted_commit"]))
        for symbol, entry in sorted((displaced or {}).items()):
            lines.append("- Reviewed displacement `%s`: metric `%s`, to `%s`, "
                         "phase `%s`, body SHA256 `%s`; evidence `%s`; "
                         "cause: %s; recovery: %s." %
                         (symbol, entry["metric"], entry["to"],
                          entry["phase"], entry["body_sha256"],
                          entry["evidence"], entry["cause"],
                          entry["recovery"]))
        for symbol, reason in sorted((rejected or {}).items()):
            lines.append("- Unreviewed loss `%s`: %s." % (symbol, reason))
    lines.append("Unreviewed byte-identical function losses fail this codegen "
                 "regression check. Explicitly reviewed losses retain their "
                 "numerical regression above; matching base SHA and current "
                 "body/relocation evidence is required. A reviewed displacement "
                 "in tools/ci/displaced.json is a separate, body-pinned "
                 "acceptance and is labelled as such. Period differential "
                 "failures remain hard failures. \"Same code\" permits "
                 "only consistent register renaming; constants, memory "
                 "operands, call/data targets, branches and operand order must "
                 "still match. Other rows are informational, with their status "
                 "showing the direction that improves the metric.")
    return lines, lost


def metrics(exact, symbols, regalloc, reloc, unresolved, same_bytes, size,
            nodata, compared, exact_bytes):
    """A complete, valid metric dict for the self-test."""
    return dict(exact=exact, exact_symbols=symbols, regalloc=regalloc,
                reloc=reloc, unresolved=unresolved, bytes=same_bytes,
                size=size, nodata=nodata, compared=compared,
                reference_functions=10, exact_bytes=exact_bytes,
                compared_bytes=1000)


def self_test():
    """Prove the direction logic accepts AND rejects before trusting a run."""
    bad = [0]
    checks = [0]

    def check(label, condition):
        checks[0] += 1
        if not condition:
            bad[0] += 1
            print("FAIL: %s" % label)

    def row(lines, label):
        for line in lines:
            if line.startswith("| %s |" % label):
                return line
        return None

    check("up-is-better: increase", status_cell(1, 1) == ":green_circle: better")
    check("up-is-better: decrease", status_cell(-1, 1) == ":red_circle: worse")
    check("down-is-better: decrease", status_cell(-1, -1) == ":green_circle: better")
    check("down-is-better: increase", status_cell(1, -1) == ":red_circle: worse")
    check("zero is no change", status_cell(0, 1) == ":white_circle: no change")
    check("zero is no change, down", status_cell(0, -1) == ":white_circle: no change")

    base = metrics(2, ["a", "b"], 1, 0, 0, 1, 1, 0, 5, 100)
    improved = metrics(3, ["a", "b", "c"], 0, 0, 0, 1, 1, 0, 5, 200)
    regressed = metrics(1, ["a"], 2, 1, 0, 1, 2, 0, 7, 50)

    lines, lost = render(base, improved)
    check("gain reports pass, bold", row(lines, "Byte-identical functions") and
          "**:green_circle: pass**" in row(lines, "Byte-identical functions"))
    check("gain loses nothing", lost == [])
    check("gain lists the new symbol",
          any("`c`" in line for line in lines))
    check("more exact bytes is better",
          ":green_circle: better" in row(lines, "Byte-identical reference code"))
    check("fewer non-exact is better",
          ":green_circle: better" in row(lines, "Not yet byte-identical"))
    check("fewer regalloc is better",
          ":green_circle: better" in
          row(lines, "Same code, different register allocation"))

    lines, lost = render(base, regressed)
    check("loss reports regression, bold",
          "**:red_circle: regression**" in
          row(lines, "Byte-identical functions"))
    check("loss names the lost symbol", lost == ["b"] and
          any("**Lost byte-identical functions:**" in line for line in lines))
    check("fewer exact bytes is worse",
          ":red_circle: worse" in row(lines, "Byte-identical reference code"))
    check("more compared is better",
          ":green_circle: better" in row(lines, "Functions compared"))
    check("more different size is worse",
          ":red_circle: worse" in row(lines, "Different size"))
    check("unchanged size is no change",
          ":white_circle: no change" in row(lines, "Could not be disassembled"))

    # --- displacement ledger: accepted, stale, and wrong-metric controls ----
    def head_body(symbol):
        return hashlib.sha256(("head:" + symbol).encode()).hexdigest()

    def displacement(symbol, metric="exact", body_hash=None,
                     selected_hash=None,
                     evidence="tools/ci/displaced.json"):
        entry = {"symbol": symbol, "metric": metric, "from": "EXACT",
                 "to": "REGALLOC", "phase": "order", "cause": "reviewed",
                 "evidence": evidence,
                 "recovery": "re-measure after the cause settles"}
        if metric in ("exact", "both"):
            entry["body_sha256"] = (body_hash if body_hash is not None
                                    else head_body(symbol))
        if metric in ("selected", "both"):
            entry["selected_body_sha256"] = (
                selected_hash if selected_hash is not None
                else head_body("selected:" + symbol))
        return entry

    matching = displacement("lost")
    accepted, rejected = displacement_losses(["lost"], [matching], head_body)
    check("matching displaced entry accepts",
          accepted == {"lost": matching} and not rejected)

    accepted, rejected = displacement_losses(
        ["lost"], [displacement("lost", body_hash="0" * 64)], head_body)
    check("wrong displacement body hash rejects",
          not accepted and "body hash does not match" in rejected["lost"])

    accepted, rejected = displacement_losses(
        ["lost"], [displacement("lost", metric="selected")], head_body)
    check("selected-only entry does not excuse an exact loss",
          not accepted and "selected-only" in rejected["lost"])

    accepted, rejected = displacement_losses(
        ["lost"], [displacement("lost",
                                evidence="tools/ci/does-not-exist.json")],
        head_body)
    check("missing displacement evidence rejects",
          not accepted and "evidence path does not exist" in rejected["lost"])

    accepted, rejected = displacement_losses(["lost"], [], head_body)
    check("an unreviewed loss still fails",
          not accepted and rejected["lost"] == "no reviewed displacement")

    accepted, rejected = displacement_losses(
        ["lost"], [displacement("lost", metric="both")], head_body)
    check("both entry with both pins accepts an exact loss",
          accepted == {"lost": displacement("lost", metric="both")}
          and not rejected)

    total = checks[0]
    print("period_metrics self-test: %d checks, %d failed" %
          (total, bad[0]))
    return 1 if bad[0] else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", help="base metrics JSON")
    ap.add_argument("--head", help="head metrics JSON")
    ap.add_argument("--check", action="store_true",
                    help="fail on any unreviewed byte-identical function loss")
    ap.add_argument("--reviewed-losses", help="explicit base/body-pinned review JSON")
    ap.add_argument("--displaced",
                    help="body-pinned displacement ledger (tools/ci/displaced.json)")
    ap.add_argument("--base-sha", help="full measured PR base revision")
    ap.add_argument("--head-objects", help="measured head's period object directory")
    ap.add_argument("--fingerprint", metavar="SYMBOL",
                    help="print a current body/relocation SHA256, then exit")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the status logic accepts and rejects, then exit")
    args = ap.parse_args()

    if args.self_test:
        return self_test()
    if args.fingerprint:
        if not args.head_objects:
            ap.error("--fingerprint requires --head-objects")
        try:
            print(function_digest(args.head_objects, args.fingerprint))
        except ValueError as exc:
            ap.error(str(exc))
        return 0
    if not args.base or not args.head:
        ap.error("--base and --head are required unless --self-test is given")

    try:
        base = load(args.base)
        head = load(args.head)
        reviews = load_reviews(args.reviewed_losses) if args.reviewed_losses else []
        displacements = load_displacements(args.displaced) if args.displaced else None
        if args.reviewed_losses and (not args.head_objects or
                not re.fullmatch(r"[0-9a-f]{40}", args.base_sha or "")):
            raise ValueError("--reviewed-losses requires --head-objects and full --base-sha")
        if args.displaced and not args.head_objects:
            raise ValueError("--displaced requires --head-objects")
    except ValueError as exc:
        ap.error(str(exc))

    lost = sorted(set(base["exact_symbols"]) - set(head["exact_symbols"]))
    digest = lambda symbol: function_digest(args.head_objects, symbol)
    accepted, rejected = review_losses(lost, reviews, args.base_sha, digest)
    displaced = None
    if displacements is not None:
        displaced, unreviewed = displacement_losses(lost, displacements, digest)
        for symbol in lost:
            if symbol in accepted:
                # A full base/body review is the stronger record; keep the
                # two acceptance paths disjoint so the label is unambiguous.
                displaced.pop(symbol, None)
                continue
            if symbol in displaced:
                rejected.pop(symbol, None)
            else:
                rejected[symbol] = unreviewed.get(
                    symbol, "no review or displacement")
    lines, lost = render(base, head, accepted, rejected, displaced)
    print("\n".join(lines))
    return 1 if args.check and rejected else 0


if __name__ == "__main__":
    sys.exit(main())
