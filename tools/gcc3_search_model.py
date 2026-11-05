#!/usr/bin/env python3
"""Conditional, bounded replay of observed GCC3 HI/SI general scratch searches.

This is apparatus, not a model of the compiler's full state. Eligibility,
allocation order, exclusions and peephole boundaries are frozen per event.
Reordered predictions require an independent trace with those same inputs.
The observer's instruction UID is retained as evidence, never as identity.

API: calibrate(trace), predict(model, order=None, start=None), search(model,
family), verify(model, order, actual_trace). Invalid evidence raises ModelError.
Families contain disjoint ``groups`` of block IDs to permute in their existing
positions, nonempty ``targets`` ({block, event: zero-based, selected: int|null}),
and an optional positive ``limit``. Unlisted blocks stay fixed. All objects
returned by this module round-trip through JSON.
"""
import argparse
import copy
import itertools
import json
import math
from pathlib import Path
import sys
import time

SLOTS = 53
SCHEMA = "gcc3-scratch-search-v1"
DEFAULT_LIMIT = 100000
OUTPUT_CAP = 10000
INPUT_KEYS = ("function", "constraint", "mode", "from", "to",
              "allocation_order", "excluded", "eligibility")
# Pinned Gentoo cc1 80a79e52...: ix86_hard_regno_mode_ok at
# 0x825a894..0x825a8d5 accepts HI=11/SI=12 for hard registers 0..7.
# Both bypass the QI-only branch. mode_size is 2/4 respectively; the
# peep2_find_free_register span at 0x81f0b58 is ceil(size/4), hence one
# live/reserved register in either mode. Pinned cc1plus b778f44b... has the
# same tests at 0x82a67a8..0x82a67e9 and span at 0x823ca6c, and the same
# mode values/classes/sizes. This does NOT establish QI, wider
# modes, or non-GPR eligibility. The same filters and frozen inputs apply.
GENERAL_REGISTER_MODES = frozenset(("HImode", "SImode"))


class ModelError(ValueError):
    """Evidence is invalid or outside this conditional model's domain."""


def _require(condition, message):
    if not condition:
        raise ModelError(message)


def _integer(value, low, high, where):
    _require(type(value) is int and low <= value <= high,
             f"{where}: expected integer in {low}..{high}, got {value!r}")


def _registers(value, where):
    _require(isinstance(value, list), f"{where}: expected register list")
    for reg in value:
        _integer(reg, 0, SLOTS - 1, where)
    _require(len(set(value)) == len(value), f"{where}: duplicate register")


def _flags(value, where):
    _require(isinstance(value, list) and len(value) == SLOTS,
             f"{where}: expected {SLOTS} flags")
    _require(all(type(flag) in (int, bool) and flag in (0, 1) for flag in value),
             f"{where}: flags must be 0 or 1")


def _event(event, where):
    _require(isinstance(event, dict), f"{where}: expected event object")
    required = INPUT_KEYS + ("cursor_before", "cursor_after", "selected")
    _require(all(key in event for key in required),
             f"{where}: missing event fields: {sorted(set(required) - event.keys())}")
    _require(isinstance(event["function"], str) and event["function"],
             f"{where}: missing function name")
    _require(event["constraint"] == "r" and isinstance(event["mode"], str)
             and event["mode"] in GENERAL_REGISTER_MODES,
             f"{where}: unsupported constraint/mode {event['constraint']!r}/{event['mode']!r}")
    for key in ("cursor_before", "cursor_after"):
        _integer(event[key], 0, SLOTS - 1, f"{where}.{key}")
    _integer(event["from"], 0, 4, f"{where}.from")
    _integer(event["to"], event["from"], 4, f"{where}.to")
    if event["selected"] is not None:
        _integer(event["selected"], 0, SLOTS - 1, f"{where}.selected")
    allocation = event["allocation_order"]
    _require(isinstance(allocation, list) and len(allocation) == SLOTS,
             f"{where}.allocation_order: expected {SLOTS} slots (duplicates allowed)")
    for reg in allocation:
        _integer(reg, 0, SLOTS - 1, f"{where}.allocation_order")
    _registers(event["excluded"], f"{where}.excluded")
    state = event["eligibility"]
    keys = {"class", "class_members", "live", "fixed", "call_used", "ever_live",
            "reload_completed", "frame_pointer_needed"}
    _require(isinstance(state, dict) and keys <= state.keys(),
             f"{where}.eligibility: missing mechanism inputs")
    _integer(state["class"], 0, 1000, f"{where}.eligibility.class")
    for key in ("class_members", "live"):
        _registers(state[key], f"{where}.eligibility.{key}")
    for key in ("fixed", "call_used", "ever_live"):
        _flags(state[key], f"{where}.eligibility.{key}")
    for key in ("reload_completed", "frame_pointer_needed"):
        _require(type(state[key]) in (int, bool) and state[key] in (0, 1),
                 f"{where}.eligibility.{key}: expected 0 or 1")
    eligible = []
    for reg in state["class_members"]:
        if state["fixed"][reg] or not state["call_used"][reg] and not state["ever_live"][reg]:
            continue
        if reg in (6, 20) and (not state["reload_completed"] or state["frame_pointer_needed"]):
            continue
        if reg in state["live"] or reg in event["excluded"]:
            continue
        _require(reg < 8, f"{where}: unsupported {event['mode']} register-mode eligibility for register {reg}")
        eligible.append(reg)
    # Candidate observations are redundant evidence, but corrupt observations
    # must not be silently ignored. No candidate count is assumed by this API.
    if "candidates" in event:
        _require(isinstance(event["candidates"], list), f"{where}.candidates: expected list")
        for index, candidate in enumerate(event["candidates"]):
            label = f"{where}.candidates[{index}]"
            _require(isinstance(candidate, dict), f"{label}: expected object")
            _require("order_index" in candidate and "register" in candidate,
                     f"{label}: missing order_index/register")
            _integer(candidate["order_index"], 0, SLOTS - 1, label)
            reg = allocation[candidate["order_index"]]
            _require(candidate["register"] == reg, f"{label}: allocation slot/register mismatch")
            expected = {"live": state["live"], "class_ok": reg in state["class_members"]}
            expected.update({key: state[key][reg] for key in ("fixed", "call_used", "ever_live")})
            expected.update({key: state[key] for key in ("reload_completed", "frame_pointer_needed")})
            for key, value in expected.items():
                if key in candidate:
                    _require(candidate[key] == value, f"{label}: changed {key}")
    return set(eligible)


def _transfer(event, start, eligible):
    for offset in range(SLOTS):
        index = (start + offset) % SLOTS
        reg = event["allocation_order"][index]
        if reg in eligible:
            return reg, (index + 1) % SLOTS
    return None, 0


def _trace(trace):
    _require(isinstance(trace, dict), "trace: expected envelope object")
    _require({"events", "errors", "exit_codes", "pending"} <= trace.keys(),
             "trace: missing events/errors/exit_codes/pending envelope fields")
    _require(trace["errors"] == [], "trace: observer reported errors")
    _require(trace["exit_codes"] == [0] and type(trace["exit_codes"][0]) is int,
             "trace: expected exactly one successful exit code [0]")
    _require(type(trace["pending"]) is int and trace["pending"] == 0,
             "trace: pending scratch searches")
    events = trace["events"]
    _require(isinstance(events, list) and events, "trace: empty/missing event stream")
    eligibility = []
    for index, event in enumerate(events):
        eligible = _event(event, f"event[{index}]")
        if index:
            _require(events[index - 1]["cursor_after"] == event["cursor_before"],
                     f"event[{index}]: cursor discontinuity")
        selection, cursor = _transfer(event, event["cursor_before"], eligible)
        _require((selection, cursor) == (event["selected"], event["cursor_after"]),
                 f"event[{index}]: selection/cursor mismatch: predicted {(selection, cursor)!r}, "
                 f"observed {(event['selected'], event['cursor_after'])!r}")
        eligibility.append(eligible)
    return events, eligibility


def _run(events, eligible, start):
    rows = []
    cursor = start
    for event, allowed in zip(events, eligible):
        selected, after = _transfer(event, cursor, allowed)
        rows.append({"selected": selected, "cursor_before": cursor, "cursor_after": after})
        cursor = after
    return {"start": start, "cursor_after": cursor, "events": rows}


def calibrate(trace):
    """Validate every baseline event and make all 53 frozen-input block maps."""
    events, eligibility = _trace(trace)
    blocks = []
    occurrences = {}
    offset = 0
    for name, group in itertools.groupby(events, key=lambda event: event["function"]):
        rows = list(group)
        occurrences[name] = occurrences.get(name, 0) + 1
        allowed = eligibility[offset:offset + len(rows)]
        blocks.append({"id": f"{name}#{occurrences[name]}", "function": name,
                       "events": copy.deepcopy(rows),
                       "transitions": [_run(rows, allowed, start) for start in range(SLOTS)]})
        offset += len(rows)
    return {"schema": SCHEMA, "conditional": True,
            "assumption": "Frozen per-event mechanism inputs; independently verify each reordered trace.",
            "trace_metadata": copy.deepcopy({key: value for key, value in trace.items() if key != "events"}),
            "baseline_start": events[0]["cursor_before"],
            "baseline_order": [block["id"] for block in blocks], "blocks": blocks,
            "counts": {"observed_events": len(events), "modeled_events": len(events),
                       "outside_model": 0, "blocks": len(blocks), "starts_per_block": SLOTS}}


def _model(model):
    _require(isinstance(model, dict) and model.get("schema") == SCHEMA,
             "model: unsupported/missing schema")
    blocks = model.get("blocks")
    _require(isinstance(blocks, list) and blocks and all(isinstance(b, dict) for b in blocks),
             "model: missing blocks")
    _require(all(isinstance(b.get("events"), list) for b in blocks), "model: missing raw events")
    metadata = model.get("trace_metadata")
    _require(isinstance(metadata, dict) and "events" not in metadata, "model: invalid trace metadata")
    trace = dict(metadata, events=[event for block in blocks for event in block["events"]])
    rebuilt = calibrate(trace)
    _require(model == rebuilt, "model: corrupted model fields or transition tables")
    return {block["id"]: block for block in blocks}


def _order(model, order):
    baseline = model["baseline_order"]
    if order is None:
        return list(baseline)
    _require(isinstance(order, list) and all(isinstance(item, str) for item in order),
             "order: expected list of block IDs")
    _require(len(order) == len(baseline) and len(set(order)) == len(order) and set(order) == set(baseline),
             "order: must contain every block ID exactly once")
    if order != baseline:
        names = [block["function"] for block in model["blocks"]]
        _require(len(names) == len(set(names)),
                 "order: repeated function names cannot be unambiguously identified after reordering")
    return list(order)


def _predict(model, blocks, order, start):
    cursor = start
    result = []
    for block_id in order:
        block = blocks[block_id]
        transition = block["transitions"][cursor]
        result.append({"id": block_id, "function": block["function"],
                       "cursor_before": cursor, "cursor_after": transition["cursor_after"],
                       "events": copy.deepcopy(transition["events"])})
        cursor = transition["cursor_after"]
    return {"conditional": True, "order": order, "start": start, "cursor_after": cursor,
            "blocks": result, "event_count": model["counts"]["modeled_events"],
            "block_count": len(result)}


def predict(model, order=None, start=None):
    """Compose whole-stream block transitions, conditional on frozen inputs."""
    blocks = _model(model)
    order = _order(model, order)
    if start is None:
        start = model["baseline_start"]
    _integer(start, 0, SLOTS - 1, "start")
    return _predict(model, blocks, order, start)


def _orders(baseline, groups, index=0):
    if index == len(groups):
        yield list(baseline)
        return
    positions = sorted(baseline.index(block_id) for block_id in groups[index])
    # Baseline order, rather than the caller's group spelling, defines traversal.
    members = [baseline[position] for position in positions]
    for permutation in itertools.permutations(members):
        candidate = list(baseline)
        for position, block_id in zip(positions, permutation):
            candidate[position] = block_id
        yield from _orders(candidate, groups, index + 1)


def search(model, family):
    """Enumerate a bounded Cartesian family; report coverage and all retained hits."""
    wall, cpu = time.perf_counter(), time.process_time()
    blocks = _model(model)
    _require(isinstance(family, dict) and {"groups", "targets"} <= family.keys(),
             "family: missing groups/targets")
    _require(set(family) <= {"groups", "targets", "limit"}, "family: unknown field")
    groups = family["groups"]
    _require(isinstance(groups, list) and groups, "family.groups: expected nonempty list")
    seen = set()
    for group in groups:
        _require(isinstance(group, list) and group and all(isinstance(item, str) for item in group),
                 "family.groups: expected nonempty block-ID lists")
        _require(len(set(group)) == len(group), "family.groups: duplicate block ID")
        _require(set(group) <= blocks.keys(), "family.groups: unknown/missing block ID")
        _require(not seen.intersection(group), "family.groups: overlapping groups")
        seen.update(group)
    targets = family["targets"]
    _require(isinstance(targets, list) and targets, "family.targets: empty targets")
    target_keys = set()
    for target in targets:
        _require(isinstance(target, dict) and set(target) == {"block", "event", "selected"},
                 "family.targets: expected block/event/selected")
        block_id = target["block"]
        _require(isinstance(block_id, str) and block_id in blocks, "family.targets: unknown block ID")
        _integer(target["event"], 0, len(blocks[block_id]["events"]) - 1, "target.event")
        if target["selected"] is not None:
            _integer(target["selected"], 0, SLOTS - 1, "target.selected")
        key = (block_id, target["event"])
        _require(key not in target_keys, "family.targets: duplicate event target")
        target_keys.add(key)
    limit = family.get("limit", DEFAULT_LIMIT)
    _require(type(limit) is int and limit > 0, "family.limit: expected positive integer")
    # Refuse ambiguous families before returning even a partial result.
    if any(len(group) > 1 for group in groups):
        reversed_order = list(model["baseline_order"])
        group = next(group for group in groups if len(group) > 1)
        a, b = (reversed_order.index(item) for item in group[:2])
        reversed_order[a], reversed_order[b] = reversed_order[b], reversed_order[a]
        _order(model, reversed_order)
    denominator = math.prod(math.factorial(len(group)) for group in groups)
    matches, match_count, evaluated = [], 0, 0
    targets_by_block = {}
    for index, target in enumerate(targets):
        targets_by_block.setdefault(target["block"], []).append((index, target["event"]))
    for order in itertools.islice(_orders(model["baseline_order"], groups), limit):
        cursor = model["baseline_start"]
        values = [None] * len(targets)
        for block_id in order:
            transition = blocks[block_id]["transitions"][cursor]
            for target_index, event_index in targets_by_block.get(block_id, ()):
                values[target_index] = transition["events"][event_index]["selected"]
            cursor = transition["cursor_after"]
        evaluated += 1
        if values == [target["selected"] for target in targets]:
            match_count += 1
            if len(matches) < OUTPUT_CAP:
                matches.append({"order": order, "target_selections": values,
                                "cursor_after": cursor})
    exhaustive = evaluated == denominator
    status = ("matches_found" if match_count else "no_match") if exhaustive else "incomplete"
    return {"conditional": True, "status": status, "family": copy.deepcopy(family),
            "denominator": denominator, "evaluated": evaluated, "exhaustive": exhaustive,
            "event_count": model["counts"]["modeled_events"], "target_count": len(targets),
            "block_count": len(blocks),
            "limit": limit, "match_count": match_count, "matches": matches,
            "output_cap": OUTPUT_CAP, "matches_truncated": match_count > len(matches),
            "wall_seconds": time.perf_counter() - wall, "cpu_seconds": time.process_time() - cpu}


def verify(model, order, actual_trace):
    """Require held-out stream order, all mechanism inputs and every outcome."""
    blocks = _model(model)
    order = _order(model, order)
    actual, unused = _trace(actual_trace)
    metadata = model["trace_metadata"]
    if "provenance" in metadata:
        _require(actual_trace.get("provenance") == metadata["provenance"],
                 "verify: missing/changed compiler, observer or profile provenance")
    prediction = _predict(model, blocks, order, model["baseline_start"])
    expected_count = model["counts"]["modeled_events"]
    _require(len(actual) == expected_count,
             f"verify: changed event count {len(actual)} != {expected_count}")
    _require(actual[0]["cursor_before"] == prediction["start"], "verify: changed initial cursor")
    offset = 0
    for block_id, predicted in zip(order, prediction["blocks"]):
        block = blocks[block_id]
        for index, (baseline, outcome) in enumerate(zip(block["events"], predicted["events"])):
            observed = actual[offset]
            for key in INPUT_KEYS:
                _require(observed[key] == baseline[key],
                         f"verify {block_id} event[{index}]: changed mechanism input {key}")
            for key in ("selected", "cursor_before", "cursor_after"):
                _require(observed[key] == outcome[key],
                         f"verify {block_id} event[{index}]: prediction mismatch {key}")
            offset += 1
    return {"verified": True, "conditional": True, "order": order,
            "verified_events": offset, "total_events": expected_count,
            "verified_blocks": len(order), "mechanism_inputs_checked": list(INPUT_KEYS),
            "insn_uid_used_as_identity": False, "prediction": prediction}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    command = commands.add_parser("calibrate")
    command.add_argument("trace", type=Path)
    command.add_argument("--output", type=Path)
    command = commands.add_parser("search")
    command.add_argument("model", type=Path)
    command.add_argument("family", type=Path)
    command.add_argument("--output", type=Path)
    command = commands.add_parser("verify")
    command.add_argument("model", type=Path)
    command.add_argument("order", type=Path, help="JSON list of all block IDs")
    command.add_argument("trace", type=Path)
    command.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    read = lambda path: json.loads(path.read_text())
    try:
        if args.command == "calibrate":
            report = calibrate(read(args.trace))
        elif args.command == "search":
            report = search(read(args.model), read(args.family))
        else:
            report = verify(read(args.model), read(args.order), read(args.trace))
        serialized = json.dumps(report, indent=2) + "\n"
        if args.output:
            args.output.write_text(serialized)
            if args.command == "calibrate":
                counts = report["counts"]
                summary = f"{counts['modeled_events']}/{counts['observed_events']} events; {counts['blocks']} blocks"
            elif args.command == "search":
                summary = (f"{report['evaluated']}/{report['denominator']} orders; "
                           f"{report['match_count']} matches; {report['target_count']} targets; "
                           f"{report['event_count']} events; {report['status']}")
            else:
                summary = f"{report['verified_events']}/{report['total_events']} events verified"
            print(f"{args.command}: {summary}; wrote {args.output}")
        else:
            print(serialized, end="")
        return 0
    except (ModelError, OSError, ValueError, TypeError, KeyError) as exc:
        print(f"{args.command}: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
