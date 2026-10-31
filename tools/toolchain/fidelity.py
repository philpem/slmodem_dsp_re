"""Diagnostic contiguous instruction runs; no equivalence or ratchet decisions.

GP register identities are abstracted, but widths, high/low byte selection,
x87 stack positions, condition mnemonics, constants, memory addressing and
canonical relocation destinations remain. Local branch byte offsets and
section-relative addends are layout-dependent and abstracted. Register
relationships and branch destinations are NOT proved by this measure.
"""
import difflib
import re

VERSION = 1
NORMALIZATION = "operand-run-v1"
REGISTERS = {"%" + name: "%r32" for name in
             ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")}
REGISTERS.update({"%" + name: "%r16" for name in
                  ("ax", "bx", "cx", "dx", "si", "di", "bp")})
REGISTERS.update({"%" + name: "%r8l" for name in ("al", "bl", "cl", "dl")})
REGISTERS.update({"%" + name: "%r8h" for name in ("ah", "bh", "ch", "dh")})
REGISTER = re.compile(r"%[a-z][a-z0-9]*(?:\([0-9]+\))?")
SECTION_ADDEND = re.compile(r"(\('section', '[^']+', )\d+(\))")
LOCAL_BRANCH = re.compile(r"^\.[+-]\d+(?= |$)")


def category(verdict):
    return "NONMATCH" if verdict in ("BYTES", "SIZE", "RELOC") else verdict


def canonical(instruction):
    mnemonic, operands = instruction
    if mnemonic.startswith(("j", "call", "loop")):
        operands = LOCAL_BRANCH.sub("<layout-target>", operands)
    operands = SECTION_ADDEND.sub(r"\1<layout-addend>\2", operands)
    operands = REGISTER.sub(lambda match: REGISTERS.get(match[0], match[0]), operands)
    return mnemonic, operands


def measure(reference, candidate, padding):
    """Input instruction lists come from byteident's relocation-aware parser."""
    left = [canonical(row) for row in reference if not padding(*row)]
    right = [canonical(row) for row in candidate if not padding(*row)]
    if not left or not right:
        raise ValueError("empty instruction stream")
    run = difflib.SequenceMatcher(a=left, b=right, autojunk=False).find_longest_match().size
    return {"reference_instructions": len(left),
            "candidate_instructions": len(right), "longest_run": run,
            "reference_percent": 100.0 * run / len(left)}


def conservative(records):
    """Any unmeasured copy prevents a complete symbol-level measurement."""
    if not records:
        raise ValueError("no defining copies")
    return min(records, key=lambda row:
               (0, 0) if row["instructions"] is None else
               (1, row["instructions"]["longest_run"]))


def summary(functions):
    measured = [row["instructions"] for row in functions.values()
                if row["instructions"] is not None]
    return {"version": VERSION, "normalization": NORMALIZATION,
            "compared_functions": len(functions), "measured_functions": len(measured),
            "unavailable_functions": len(functions) - len(measured),
            "longest_run_instructions": sum(row["longest_run"] for row in measured),
            "reference_instructions": sum(row["reference_instructions"] for row in measured)}


def rank(row):
    """Largest reference coverage first; unavailable records come last."""
    run = row["instructions"]
    return ((1, 0, 0) if run is None else
            (0, -run["reference_percent"], -run["longest_run"]))


def print_rows(functions, limit, out=print):
    nonmatch = [(symbol, row) for symbol, row in functions.items()
                if row["fidelity"] == "NONMATCH"]
    rows = sorted((item for item in nonmatch if item[1]["instructions"] is not None),
                  key=lambda item: (rank(item[1]), item[0]))
    out("\nNONMATCH: %d functions; %d measured, %d unavailable; showing %d" %
        (len(nonmatch), len(rows), len(nonmatch) - len(rows), min(limit, len(rows))))
    out("  Ranked by longest contiguous instruction run / reference (diagnostic).")
    out("  %-23s %10s %10s %10s %10s  %s" %
        ("run / ref instructions", "our insns", "ref bytes", "our bytes", "byte delta", "symbol"))
    for symbol, row in rows[:limit]:
        run = row["instructions"]
        value = ("unavailable" if run is None else "%d / %d (%.1f%%)" %
                 (run["longest_run"], run["reference_instructions"], run["reference_percent"]))
        out("  %-23s %10d %10d %10d %+10d  %s" %
            (value, run["candidate_instructions"], row["reference_bytes"],
             row["candidate_bytes"], row["byte_delta"], symbol))
