#!/usr/bin/env python3
"""What our evidence actually PROVES about a reimplemented symbol.

WHY THIS EXISTS

The tree measures BYTES well.  `byteident.py` says how many symbols are the
same bytes in the same places (grade 0) and how many are the same instructions
under a register renaming (grade 1).  Neither number answers the question a
reader of this reconstruction actually asks:

    where we have reimplemented something and it is NOT byte-exact,
    what proves it computes the same function?

That is a claim about SEMANTICS, and the only apparatus this tree has for it
is the differential tier -- our function and the blob's, driven side by side
on chosen inputs.  A differential test proves equivalence **over the inputs it
drives** and over nothing else.  So the honest report is not "tested / not
tested"; it is a partition of the population by what kind of evidence exists,
with a denominator on every class.

`docs/method/equivalence.md` already settled the other route and the answer
was NO: on the only lifter installed there is no rung that is both faithful
and more normalised than the instruction streams `byteident.py` already
compares.  This tool does not relitigate that.  It measures coverage of the
differential tier instead.

THIS IS A TRIAGE AID, NEVER A GATE, in `extcheck.py`'s exact sense.  It grades
nothing, closes nothing, and no finding may cite a class from it as a
derivation.  What it produces is a list with a denominator.

TWO QUESTIONS, TWO MODES

  --grade1   Is grade 1 a PROOF?  A consistent renaming of registers preserves
             semantics, so a grade-1 symbol is arguably proven equivalent by
             construction rather than by testing.  This mode states the four
             places where that argument has a hole and CENSUSES THE REAL
             GRADE-1 POPULATION for each.  A synthetic control shows each
             probe firing before any clean run from it is believed (F134).

  (default)  Everything that is neither grade 0 nor grade 1 needs a coverage
             argument.  This mode gives that population five sections:

             I.   WHERE THE EVIDENCE COMES FROM.
                  DIRECT     a compiled test object references `ref_SYMBOL`,
                             so the symbol is compared against the blob AT ITS
                             OWN BOUNDARY, on inputs the test chose.
                  COMPOSITE  no test names it, but our build reaches it from a
                             DIRECT symbol.  It IS differentially compared --
                             in composition, through a caller, on whatever
                             inputs that caller's test happens to induce, and
                             only as far as the caller's output reveals.  Real
                             evidence, much weaker, and never argued anywhere.
                  NONE       neither.  No differential evidence of any kind.

                  `coverage.py`'s "tested" line counts DIRECT only, which is
                  why F8326 says a member reached through a tested dispatcher
                  reads untested there.  This section is that finding
                  measured, and the answer is three symbols.

             II.  WHAT IT PROVES: exhaustive over a whole domain, exhaustive
                  over a reduced one, sampled, ad-hoc, untested.  A partition.
             III. HOW MANY INPUTS -- the check counts, banded.
             IV.  DISCRIMINATING POWER -- the mutation verdicts, which are the
                  only thing here that speaks to whether a test could TELL.
             V.   WHAT IT WOULD COST to move the weak class up.

WHAT IT CANNOT DO, STATED SO NOBODY READS MORE INTO IT

  - **It cannot decide EXHAUSTIVE.**  Nothing in this tree can compute a
    function's input domain; a function taking a `struct *` has a reachable
    state space, not a domain.  Exhaustiveness is DECLARED by the test that
    claims it (`t_fpm_phasordp`'s header is the model) and verified by
    READING it.  This tool harvests the claims; the WHOLE/REDUCED split
    between them is a hand result carried in `VERIFIED_CLAIMS` and marked as
    such.  Inferring exhaustiveness from a check count would be the dead
    detector this tree keeps rediscovering.
  - **Check counts are per BINARY, not per symbol.**  A binary driving three
    symbols reports one number for all three.  Quoted as the binary's.
  - **Reachability is an over-approximation.**  An edge is any relocation from
    one function's body to another symbol, so an address stored in a table
    counts.  That is the right direction for a COMPOSITE claim -- it can only
    make the class larger, i.e. make the evidence sound WEAKER than reported,
    never stronger.

Usage:
    tools/eqproof.py                 the classification, with denominators
    tools/eqproof.py --grade1        the grade-1 proof census
    tools/eqproof.py --class DIRECT  one class, listed
    tools/eqproof.py --selftest      every probe shown firing
    tools/eqproof.py --sym NAME      one symbol, with its evidence
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

#
# `tools/dis.py` SHADOWS THE STANDARD LIBRARY'S `dis`, which `inspect` imports
# on the way up from almost anything.  Run out of this directory a traceback
# cannot even be FORMATTED -- the crash handler dies with
# `module 'dis' has no attribute 'COMPILER_FLAG_NAMES'` and prints that
# instead of the real error, naming neither this directory nor the file.  The
# fix is `whichfield.py`'s: drop our own directory from the path.  It has to
# come before `toolchain` goes on, or the entry we add is the one removed.
#
sys.path[:] = [p for p in sys.path if os.path.abspath(p or ".") != HERE]
sys.path.insert(0, os.path.join(HERE, "toolchain"))

import byteident as BI                                        # noqa: E402


# ----------------------------------------------------------------- grade 1

#
# THE i386 SysV ABI SPLIT.  %ebx, %esi, %edi and %ebp are callee-saved: a
# function must give them back unchanged.  %eax, %ecx and %edx are the
# caller's problem and a `call` may destroy them.  `alpha_equal` pins %esp and
# %ebp to themselves and lets the other six rename freely, which is right for
# a value that lives and dies inside the function and WRONG for one that has
# to survive a call or be visible at `ret`.
#
CALLEE_SAVED = frozenset(("ebx", "esi", "edi", "ebp"))
RETURN_REGS = ("eax", "edx")

# Any conditional or unconditional intra-function branch.
BRANCH = re.compile(r"^(jmp|j[a-z]+|loop[a-z]*)$")
SELFREL = re.compile(r"^\.([-+]\d+)$")


def rows_with_offsets(path, sym):
    """byteident's rows, plus each row's byte offset from the function start.

    THE ROWS THEMSELVES COME FROM `byteident.insns`, NOT FROM A SECOND
    DISASSEMBLY.  A second copy of a comparison is a second answer to the same
    question, and this tool must never disagree with the tool that assigns the
    grade.  The offsets are recovered from a parallel objdump run and matched
    to the rows POSITIONALLY, and the count is asserted -- if the two ever
    drift, this raises rather than aligning the wrong instruction with the
    wrong offset.
    """
    rows = BI.insns(path, sym)
    out = subprocess.run(
        ["objdump", "-d", "--disassemble=" + sym, "--no-show-raw-insn", path],
        capture_output=True, text=True).stdout
    offs, base = [], None
    for line in out.splitlines():
        m = re.match(r"^\s*([0-9a-f]+):\t", line)
        if not m:
            continue
        at = int(m.group(1), 16)
        if base is None:
            base = at
        offs.append(at - base)
    if len(offs) != len(rows):
        raise RuntimeError("eqproof: %s in %s -- %d rows against %d offsets; "
                           "the two objdump passes disagree and no alignment "
                           "may be assumed" % (sym, path, len(rows), len(offs)))
    return [(r[0], r[1], o) for r, o in zip(rows, offs)]


UNCOND = re.compile(r"^(jmp|ret[a-z]*|hlt|ud2)$")


def back_edge_spans(rows):
    """[(loop_head_off, back_branch_off)] for every TRUE loop back edge.

    A BACK EDGE IS NOT "A BRANCH THAT POINTS BACKWARDS IN THE TEXT", and the
    difference is not academic.  GCC 3.4.2 puts a cold path -- an allocation
    failure, an early-out -- at the END of the function and jumps back into the
    main line from it.  `_iir_filter_create` does exactly that:
    `je +0xb0` at the top, and `jmp +0x23` at +0xbe once the allocation is
    done.  Textually that is a backward branch spanning almost the whole
    function; in the control-flow graph it is a MERGE that executes at most
    once, and the state re-entering at +0x23 is the state that left the top,
    not the state at +0xbe.

    Reading it as a loop reported the one LOOPREBIND hazard in the whole
    grade-1 population, and the hazard was this tool's and not the code's.

    So: build the basic blocks, compute dominators, and take only the edges
    u -> v where v DOMINATES u.  That is the definition of a back edge and it
    is the only one that means "this executes again with a state that came
    round".
    """
    if not rows:
        return []
    off = [r[2] for r in rows]
    index = {o: i for i, o in enumerate(off)}
    tgts = {}
    for i, (mn, ops, o) in enumerate(rows):
        if not BRANCH.match(mn):
            continue
        m = SELFREL.fullmatch(ops.strip())
        if m:
            tgts[i] = int(m.group(1))
    #
    # LEADERS: the entry, every branch target, and every row after a branch or
    # a return.  A target that lands between two instructions is not a leader
    # -- it cannot happen in a well-formed function, and silently rounding one
    # would build a graph for code that is not there.
    #
    leaders = {0}
    for i, (mn, ops, o) in enumerate(rows):
        if i in tgts:
            if tgts[i] in index:
                leaders.add(index[tgts[i]])
            if i + 1 < len(rows):
                leaders.add(i + 1)
        elif UNCOND.match(mn) and i + 1 < len(rows):
            leaders.add(i + 1)
    order = sorted(leaders)
    blk = {}
    for n, ld in enumerate(order):
        hi = order[n + 1] if n + 1 < len(order) else len(rows)
        for i in range(ld, hi):
            blk[i] = n
    succ = {n: set() for n in range(len(order))}
    for n, ld in enumerate(order):
        hi = order[n + 1] if n + 1 < len(order) else len(rows)
        last = hi - 1
        mn = rows[last][0]
        if last in tgts and tgts[last] in index:
            succ[n].add(blk[index[tgts[last]]])
        if not UNCOND.match(mn) and hi < len(rows):
            succ[n].add(blk[hi])
    pred = {n: set() for n in range(len(order))}
    for a, bs in succ.items():
        for b in bs:
            pred[b].add(a)
    #
    # Iterative dominators.  The graph is a few dozen nodes; the simple
    # fixpoint is instant and needs no library.
    #
    allb = set(succ)
    dom = {n: (set([0]) if n == 0 else set(allb)) for n in allb}
    changed = True
    while changed:
        changed = False
        for n in sorted(allb):
            if n == 0:
                continue
            ps = [dom[p] for p in pred[n] if p in dom]
            new = set.intersection(*ps) if ps else set()
            new = new | {n}
            if new != dom[n]:
                dom[n] = new
                changed = True
    spans = []
    for u, bs in succ.items():
        for v in bs:
            if v in dom[u] and v <= u:
                spans.append((off[order[v]], off[
                    (order[u + 1] - 1) if u + 1 < len(order) else len(rows) - 1]))
    return sorted(set(spans))


def replay(x, y):
    """Re-run `alpha_why`'s binding, recording WHERE each binding was made.

    THIS DOES NOT DECIDE GRADE 1 AND MUST NOT.  It is only ever run on a pair
    `byteident.alpha_equal` has already ACCEPTED, and it reports back whether
    it reached the end without a conflict.  A disagreement means this replay
    has drifted from the tool that assigns the grade, and `--selftest` fails on
    it rather than quietly reporting hazards computed from the wrong bindings.

    Returns (ok, events, trace) where an event is one of

        ("bind", i, blob_reg, ours_reg)     a use pinned the pair at row i
        ("rebind", i, blob_reg, ours_reg)   a full write rebound it at row i
        ("ret", i, dict(fwd))               a `ret`, with the map as it stood
        ("call", i, dict(fwd))              a `call`, likewise

    and `trace` is one entry per PADDING-STRIPPED row:

        {"mn": mnemonic, "uses": {blob regs read}, "def": blob reg written
         or None, "pre": the map as it stood BEFORE the row}

    The trace is what makes the two liveness-sensitive hazards decidable.
    Without it the only questions this could ask are "did a rebind happen
    somewhere in a loop" and "is there a call anywhere in the body", and both
    of those fire on functions where nothing is wrong.
    """
    x = [r for r in x if not BI._padding(r[0], r[1])]
    y = [r for r in y if not BI._padding(r[0], r[1])]
    if len(x) != len(y):
        return False, [], []
    fwd, rev = {"esp": "esp", "ebp": "ebp"}, {"esp": "esp", "ebp": "ebp"}
    events, trace = [], []

    def bind(fu, fv):
        return fwd.setdefault(fu, fv) == fv and rev.setdefault(fv, fu) == fu

    def rebind(fu, fv):
        if fu in ("esp", "ebp") or fv in ("esp", "ebp"):
            return fu == fv
        old = fwd.pop(fu, None)
        if old is not None:
            rev.pop(old, None)
        old = rev.pop(fv, None)
        if old is not None:
            fwd.pop(old, None)
        fwd[fu], rev[fv] = fv, fu
        return True

    for i, ((mx, ox), (my, oy)) in enumerate(zip(x, y)):
        if mx != my:
            return False, events, trace
        row = {"mn": mx, "uses": set(), "def": None, "pre": dict(fwd)}
        trace.append(row)
        if BI._padding(mx, ox) and BI._padding(my, oy):
            continue
        if mx == "ret" or mx.startswith("ret"):
            events.append(("ret", i, dict(fwd)))
        if mx == "call" or mx.startswith("call"):
            events.append(("call", i, dict(fwd)))
        fx, fy = BI._fields(ox), BI._fields(oy)
        if len(fx) != len(fy):
            return False, events, trace
        dx = fx[-1].strip() if fx else ""
        dy = fy[-1].strip() if fy else ""
        idiom = (mx in ("xor", "sub", "sbb") and len(fx) == 2
                 and fx[0].strip() == dx and fy[0].strip() == dy
                 and BI._full_write(dx) and BI._full_write(dy))
        isdef = bool(fx) and BI._full_write(dx) and BI._full_write(dy) and (
            mx in BI.DEFS or mx.startswith("set") or mx.startswith("cmov")
            or idiom)
        ux = ([] if idiom else fx[:-1]) if isdef else fx
        uy = ([] if idiom else fy[:-1]) if isdef else fy
        rx = BI.REGTOK.findall(" ".join(ux))
        ry = BI.REGTOK.findall(" ".join(uy))
        if len(rx) != len(ry):
            return False, events, trace
        for u, v in zip(rx, ry):
            fu, fv = BI.REG32.get(u), BI.REG32.get(v)
            if fu is None or fv is None:
                if u != v:
                    return False, events, trace
                continue
            if len(u) != len(v):
                return False, events, trace
            if not bind(fu, fv):
                return False, events, trace
            row["uses"].add(fu)
            events.append(("bind", i, fu, fv))
        if isdef:
            u, v = dx[1:], dy[1:]
            fu, fv = BI.REG32.get(u), BI.REG32.get(v)
            if fu is None or fv is None:
                if u != v:
                    return False, events, trace
            elif len(u) != len(v) or not rebind(fu, fv):
                return False, events, trace
            else:
                row["def"] = fu
                events.append(("rebind", i, fu, fv))
        if BI.REGTOK.sub("%r", ox) != BI.REGTOK.sub("%r", oy):
            return False, events, trace
    #
    # ONE EXTRA ENTRY, so the state AFTER the last row can be read the same
    # way as the state before any other.  Without it the back-edge check has
    # no map to compare the loop head against when the branch is the final
    # instruction.
    #
    trace.append({"mn": "", "uses": set(), "def": None, "pre": dict(fwd)})
    return True, events, trace


#
# THE FOUR HAZARDS.  Each is a place where "the same instructions under a
# consistent register renaming" is NOT sufficient for "the same function".
# Each is stated so it can be checked mechanically, and each is a HAZARD SHAPE
# rather than a defect: it says the by-construction argument does not close on
# its own here, so the symbol needs reading.  Over-approximating is the safe
# direction -- it can only ever say a proof is INCOMPLETE where it is fine.
#
HAZARDS = {
    "RET": "a return register is bound to a different register at `ret`",
    "SAVECLASS": "the bijection crosses the callee-saved/caller-saved line",
    "CALLLIVE": "it crosses that line while a `call` stands between",
    "LOOPREBIND": "a register is rebound inside a loop body, and the "
                  "binding validated on the first pass need not hold on the "
                  "second",
}


def hazards(x, y, rows=None):
    """{hazard: [witness, ...]} for one grade-1 pair.  Empty dict is clean."""
    ok, events, trace = replay(x, y)
    if not ok:
        return {"REPLAY": ["this tool's replay disagrees with alpha_equal"]}
    found = {}

    def note(k, w):
        found.setdefault(k, [])
        if w not in found[k]:
            found[k].append(w)

    #
    # (a) RET.  `ret` carries no operands, so nothing in `alpha_why` pins the
    # binding at the one point where a register's value LEAVES the function.
    # If %eax is bound to anything but %eax where the function returns, the
    # blob returns what ITS %eax holds and ours returns what OUR %eax holds --
    # and our %eax is the image of some other blob register, or of nothing.
    #
    # THIS SHAPE IS VACUOUS FOR A FUNCTION RETURNING void, and no amount of
    # reading the instruction stream can tell the two apart: a void function
    # leaves whatever it likes in %eax and nothing may read it.  The caller
    # splits the population on the DECLARED return type, from our own headers,
    # and reports the split rather than folding the two together.
    #
    for e in [e for e in events if e[0] == "ret"]:
        i, m = e[1], e[2]
        for r in RETURN_REGS:
            if m.get(r, r) != r:
                note("RET", "row %d: %s -> %s at ret" % (i, r, m[r]))

    #
    # (b) SAVECLASS and (c) CALLLIVE.  The ABI does not treat the six free
    # registers alike.  A blob value in %ebx survives a call; the same value in
    # our %eax does not.
    #
    # SAVECLASS ON ITS OWN IS NOT A DEFECT AND MUST NOT BE READ AS ONE.  A
    # callee-saved register used as pure scratch inside one live range, with no
    # call in it, is interchangeable with a caller-saved one -- and both sides
    # save whatever their own ABI obliges them to, which `alpha_equal` already
    # checks by matching the push/pop instructions.  It is reported because it
    # is the PRECONDITION for the one that bites, and because a reader should
    # see how much of the population is even eligible.
    #
    # CALLLIVE is the one that bites, and it is a LIVENESS question rather than
    # a presence question: the crossing binding must still stand at a `call`,
    # AND the blob-side register must be READ after that call before anything
    # rewrites it.  The first draft asked only whether the function contained a
    # call anywhere, which fires on functions where the crossing range ended
    # long before the call and nothing is wrong.
    #
    for e in [e for e in events if e[0] in ("bind", "rebind")]:
        i, b, o = e[1], e[2], e[3]
        if b == o:
            continue
        if (b in CALLEE_SAVED) == (o in CALLEE_SAVED):
            continue
        note("SAVECLASS", "row %d: %s -> %s" % (i, b, o))
        for c in [ev[1] for ev in events if ev[0] == "call" and ev[1] > i]:
            if c >= len(trace) or trace[c].get("pre", {}).get(b) != o:
                continue                       # the range ended before the call
            #
            # READ AFTER THE CALL?  Scanned forward linearly, which
            # over-approximates across a branch -- the safe direction here,
            # since it can only ever say the argument is INCOMPLETE.
            #
            for j in range(c + 1, len(trace)):
                if b in trace[j]["uses"]:
                    note("CALLLIVE",
                         "row %d: %s -> %s, still bound at the call on row %d "
                         "and %s is read again on row %d" % (i, b, o, c, b, j))
                    break
                if trace[j]["def"] == b:
                    break

    #
    # (d) LOOPREBIND.  `alpha_why` is a LINEAR SCAN with no control-flow graph,
    # so it validates ONE pass through the instruction list.  A back edge means
    # the second pass begins with the bindings the first pass ENDED with, and
    # those are not the ones it checked.
    #
    # THE TEST IS NOT "was there a rebind in the body".  A rebind that leaves
    # the map where it found it changes nothing, and an identity rebind
    # (%ebx -> %ebx) is the commonest thing in a loop.  What matters is whether
    # the map DIFFERS between the loop head and the back edge, and differs for
    # a register the body READS BEFORE WRITING -- a loop-carried value.  Asking
    # the loose question reported 16 of 32 where the sharp one reports far
    # fewer, and every one of the extras was an identity rebind.
    #
    if rows is not None:
        spans = back_edge_spans(rows)
        keep = [o for (m, p, o) in rows if not BI._padding(m, p)]
        for tgt, br in spans:
            head = next((k for k, o in enumerate(keep) if o >= tgt), None)
            tail = next((k for k, o in enumerate(keep) if o == br), None)
            if head is None or tail is None or tail < head:
                continue
            if head >= len(trace) or tail + 1 >= len(trace):
                continue
            live_in, written = set(), set()
            for j in range(head, tail + 1):
                live_in |= (trace[j]["uses"] - written)
                if trace[j]["def"]:
                    written.add(trace[j]["def"])
            mh, mb = trace[head]["pre"], trace[tail + 1]["pre"]
            for r in sorted(live_in):
                if mh.get(r) != mb.get(r):
                    note("LOOPREBIND",
                         "loop +0x%x..+0x%x carries %s, mapped to %s at the "
                         "head and to %s at the back edge"
                         % (tgt, br, r, mh.get(r), mb.get(r)))
    return found


#
# DOES IT RETURN A VALUE AT ALL?  The RET shape is vacuous for a function
# returning void: such a function may leave anything in %eax and no conforming
# caller may read it, so a binding that renames %eax at `ret` costs nothing.
# Nothing in the instruction stream distinguishes the two cases -- the answer
# is in the SOURCE, and we have the source, because we wrote it.
#
# Two rules and an honest UNKNOWN:
#
#   a mangled C1/C2/C3/D0/D1/D2 name is a constructor or a destructor, and the
#   LANGUAGE says those have no return value.  That is not an ABI guess.
#
#   anything else is looked up by its declaration in our own headers and
#   sources.  What is not found says UNKNOWN and is counted as a hazard, which
#   is the safe direction: an unresolved symbol must not read as cleared.
#
CTORDTOR = re.compile(r"(C[123]|D[012])E[a-zA-Z0-9_]*$")


#
# MEMOISED, because this is called per SYMBOL and it forks `c++filt`.  Over
# 673 symbols the unmemoised version turned a two-second report into minutes,
# and the classifier calls it more than once per symbol.
#
_DEMANGLED = {}


def _demangled(sym):
    if not sym.startswith("_Z"):
        return sym
    if sym not in _DEMANGLED:
        out = subprocess.run(["c++filt", "--no-strip-underscore", sym],
                             capture_output=True, text=True).stdout.strip()
        _DEMANGLED[sym] = out or sym
    return _DEMANGLED[sym]


_DECL_CACHE = {}


def _decl_text():
    """Every line of include/ and src/, once.  Grepping 16 symbols one at a
    time over 400 files is 16 walks of the tree for one walk's worth of data."""
    if "lines" not in _DECL_CACHE:
        lines = []
        for base in ("include", "src"):
            for root, _, names in os.walk(os.path.join(ROOT, base)):
                for n in names:
                    if not n.endswith((".h", ".c", ".cpp", ".hpp")):
                        continue
                    try:
                        with open(os.path.join(root, n), errors="replace") as f:
                            lines.extend(f.read().splitlines())
                    except OSError:
                        pass
        _DECL_CACHE["lines"] = lines
    return _DECL_CACHE["lines"]


def _files():
    """(path, [lines]) for every header and source in the reconstruction."""
    if "files" not in _DECL_CACHE:
        out = []
        for base in ("include", "src"):
            for root, _, names in os.walk(os.path.join(ROOT, base)):
                for n in sorted(names):
                    if not n.endswith((".h", ".c", ".cpp", ".hpp")):
                        continue
                    fp = os.path.join(root, n)
                    try:
                        with open(fp, errors="replace") as f:
                            out.append((fp, f.read().splitlines()))
                    except OSError:
                        pass
        _DECL_CACHE["files"] = out
    return _DECL_CACHE["files"]


def _argc(text):
    """Arguments in `a, b, c` at nesting depth 0.  `None` for an empty list."""
    text = text.strip()
    if not text or text == "void":
        return 0
    n, depth = 1, 0
    for ch in text:
        if ch in "(<[":
            depth += 1
        elif ch in ")>]":
            depth -= 1
        elif ch == "," and depth == 0:
            n += 1
    return n


def _lead_type(line, name):
    """The return type written before `name(` on this line, or None."""
    m = re.search(r"([A-Za-z_][A-Za-z0-9_:<>,&\s]*?[\s*&]+)%s\s*\("
                  % re.escape(name), line)
    if not m:
        return None
    lead = m.group(1).strip()
    if lead.split()[-1] in ("return", "if", "while", "for", "switch", "else",
                            "case", "sizeof"):
        return None
    #
    # STRIP THE QUALIFIER, TEMPLATE ARGUMENTS AND ALL.  An out-of-line member
    # definition is spelt `void Scrambler<T, I>::reset(T value)`, so the text
    # before the method name ends in a class name and not in a type.  Reading
    # the last word of that gave `I>::` and scored the function as returning a
    # VALUE -- and `Scrambler<h,h>::reset` really is declared `void`, so the
    # one member the mangling could not settle was the one it got wrong.
    #
    while True:
        stripped = re.sub(r"\s*[A-Za-z_]\w*\s*(<[^<>]*>)?\s*::\s*$", "", lead)
        if stripped == lead:
            break
        lead = stripped
    if not lead:
        return None
    if "*" in lead or "&" in lead:
        return "VALUE"
    words = [w for w in lead.replace("*", " ").split()
             if w not in ("static", "inline", "extern", "virtual", "const",
                          "template", "explicit", "friend")]
    if not words:
        return None
    return "VOID" if words[-1] == "void" else "VALUE"


def _lead_type_args(line, name):
    """How many arguments the declaration of `name` on this line takes."""
    i = line.find(name + "(")
    if i < 0:
        i = line.find(name + " (")
    if i < 0:
        return None
    j = line.find("(", i)
    depth, k = 0, j
    while k < len(line):
        if line[k] == "(":
            depth += 1
        elif line[k] == ")":
            depth -= 1
            if depth == 0:
                return _argc(line[j + 1:k])
        k += 1
    return None


def returns_value(sym):
    """"VOID", "VALUE" or "UNKNOWN" for one symbol.

    UNKNOWN counts as a hazard wherever this is used.  An unresolved lookup
    must never read as cleared -- that is the direction in which a detector
    lies about a clean tree.
    """
    #
    # 1. A CONSTRUCTOR OR DESTRUCTOR HAS NO RETURN VALUE.  A language rule, not
    #    an ABI guess: no caller can name the value, so nothing may read it.
    #
    if sym.startswith("_Z") and CTORDTOR.search(sym):
        return "VOID"
    d = _demangled(sym)
    #
    # 2. A TEMPLATE'S MANGLED NAME CARRIES ITS RETURN TYPE, because the return
    #    type participates in overload resolution there.  `_Z7hammingIfEvPT_j`
    #    demangles to `void hamming<float>(float*, unsigned int)` and the
    #    answer is free and exact.
    #
    if d.startswith("void ") and "(" in d:
        return "VOID"
    #
    # A SPACE BEFORE THE PARENTHESIS MEANS A RETURN TYPE WAS MANGLED IN -- but
    # only once the TEMPLATE ARGUMENTS are taken out first.  `c++filt` renders
    # `_ZN9ScramblerIhhE5resetEh` as
    # `Scrambler<unsigned char, unsigned char>::reset(unsigned char)`, whose
    # spaces are all inside the angle brackets, and reading one of those as a
    # return type scored a `void` member as returning a value.
    #
    head = re.sub(r"<[^<>]*>", "", d.split("(")[0])
    while "<" in head and ">" in head:
        h2 = re.sub(r"<[^<>]*>", "", head)
        if h2 == head:
            break
        head = h2
    if d != sym and not d.startswith(("void ", "(")) and " " in head.strip():
        return "VALUE"
    name = d.split("(")[0].strip()
    short = name.split("::")[-1].split("<")[0].strip()
    cls = name.split("::")[0].split("<")[0].strip() if "::" in name else None
    if not re.fullmatch(r"[A-Za-z_~][A-Za-z0-9_]*", short):
        return "UNKNOWN"
    #
    # 3. A MEMBER IS LOOKED UP INSIDE ITS OWN CLASS.  The first draft searched
    #    every line of include/ and src/ for the bare method name and got
    #    UNKNOWN for almost everything, because `reset` is declared by
    #    seventeen classes and half of them return a value.  An unqualified
    #    grep over a tree with a class hierarchy in it answers a different
    #    question from the one asked.
    #
    scope = None
    if cls:
        want = re.compile(r"\b(class|struct)\s+%s\b" % re.escape(cls))
        scope = [(f, ls) for f, ls in _files() if any(want.search(l) for l in ls)]
        if not scope:
            scope = None
    want_argc = _argc(d.split("(", 1)[1].rsplit(")", 1)[0]) if "(" in d else None
    seen, by_arity = set(), set()
    for f, ls in (scope if scope is not None else _files()):
        for line in ls:
            st = line.strip()
            if short not in st or st.startswith(("//", "*", "/*", "#")):
                continue
            #
            # An out-of-line definition is qualified and beats everything else,
            # because it can only be this function.
            #
            if cls and (cls + "::" + short) in st:
                t = _lead_type(st, cls + "::" + short)
                if t:
                    return t
            t = _lead_type(st, short)
            if t:
                #
                # AN OVERLOAD SET IS NOT AMBIGUOUS -- IT IS INDEXED BY ITS
                # ARGUMENTS, and the mangled name carries them.  `Scrambler`
                # declares both `T process(T in)` and
                # `void process(const T *, I *, unsigned)`, so dropping the
                # argument list turned the one symbol whose answer decides a
                # hazard into an UNKNOWN.  Match the arity from the demangling
                # and only fall back to the whole set when nothing does.
                #
                seen.add(t)
                if want_argc is not None:
                    got = _lead_type_args(st, short)
                    if got is not None and got == want_argc:
                        by_arity.add(t)
    if len(by_arity) == 1:
        return by_arity.pop()
    if len(seen) == 1:
        return seen.pop()
    return "UNKNOWN"


def grade_partition(verbose=False):
    """(grade0, grade1, rest, objs) over byteident's own denominator."""
    blob = BI.sizes(BI.BLOB)
    ours, allobjs = {}, {}
    for o in sorted(glob.glob(os.path.join(BI.OURS, "*.o"))):
        for k, v in BI.sizes(o).items():
            ours.setdefault(k, o)
            allobjs.setdefault(k, []).append(o)
    if not blob:
        sys.exit("eqproof.py: NO SYMBOLS read from the blob at %s.  Every "
                 "count below would be computed against nothing.  F2400."
                 % BI.BLOB)
    if not ours:
        sys.exit("eqproof.py: no objects in %s -- run "
                 "tools/toolchain/build.sh first." % BI.OURS)
    stale_src, stale_obj = BI._staleness()
    if stale_src:
        sys.exit("eqproof.py: %s IS STALE (%s is newer than %s).  Run "
                 "tools/toolchain/build.sh." % (BI.OURS, stale_src, stale_obj))
    RANK = {"EXACT": 0, "UNRESOLVED": 1, "REGALLOC": 2, "RELOC": 3,
            "BYTES": 4, "SIZE": 5, "NODATA": 6}
    g0, g1, rest = [], [], []
    common = sorted(k for k in ours if k in blob)
    for k in common:
        ab, ar = BI.body(BI.BLOB, k)
        scored = []
        for o in allobjs.get(k, [ours[k]]):
            bb2, br2 = BI.body(o, k)
            scored.append((BI.verdict(ab, ar, bb2, br2), o))
        (v, n), worst = max(scored, key=lambda s: RANK.get(s[0][0], 9))
        ours[k] = worst
        if v in ("EXACT", "UNRESOLVED"):
            g0.append(k)
        elif v == "NODATA":
            rest.append(k)
        elif v != "RELOC" and BI.alpha_equal(BI.insns(BI.BLOB, k),
                                             BI.insns(worst, k)):
            g1.append(k)
        else:
            rest.append(k)
    return g0, g1, rest, ours


def grade1_census(quiet=False):
    g0, g1, rest, objs = grade_partition()
    n = len(g0) + len(g1) + len(rest)
    print("  blob : %s" % BI.BLOB)
    print("  ours : %s\n" % BI.OURS)
    print("Grade 1 as a PROOF -- the census, over %d symbol(s) both define.\n"
          % n)
    print("  grade 0 (EXACT or UNRESOLVED) : %4d   -- not this question" % len(g0))
    print("  grade 1 (REGALLOC)            : %4d   <- the population here" % len(g1))
    print("  neither                       : %4d   -- the coverage question\n"
          % len(rest))
    if not g1:
        sys.exit("eqproof.py: the grade-1 population is EMPTY, so every count "
                 "below is over nothing.  That is a refusal, not a clean run.")
    tally = {k: [] for k in HAZARDS}
    tally["REPLAY"] = []
    for k in g1:
        try:
            rw = rows_with_offsets(objs[k], k)
        except RuntimeError:
            rw = None
        h = hazards(BI.insns(BI.BLOB, k), BI.insns(objs[k], k), rw)
        for hz, ws in h.items():
            tally.setdefault(hz, []).append((k, ws[0]))
    #
    # THE RET SHAPE IS VACUOUS FOR A void FUNCTION, so it is split rather than
    # totalled.  Folding the two together would report a hazard count that is
    # mostly constructors and `void reset()` members -- true of the SHAPE and
    # false of the tree.
    #
    ret_void, ret_value, ret_unk = [], [], []
    for k, w in tally.get("RET", []):
        rv = returns_value(k)
        (ret_void if rv == "VOID" else
         ret_value if rv == "VALUE" else ret_unk).append((k, w))
    tally["RET"] = ret_value + ret_unk

    print("  hazard shapes over the %d grade-1 symbol(s):\n" % len(g1))
    print("    %-11s %4d of %d   %s"
          % ("RET", len(ret_void) + len(ret_value) + len(ret_unk), len(g1),
             HAZARDS["RET"]))
    print("      of which the symbol returns void, so the shape is VACUOUS"
          "  : %d" % len(ret_void))
    print("                      the symbol returns a VALUE, so it is REAL "
          "     : %d" % len(ret_value))
    print("                      our source does not settle it              "
          "     : %d" % len(ret_unk))
    if not quiet:
        for k, w in (ret_value + ret_unk)[:12]:
            print("                            %s  [%s]" % (k[:56], w))
    for hz in ("SAVECLASS", "CALLLIVE", "LOOPREBIND", "REPLAY"):
        rows = tally.get(hz, [])
        label = HAZARDS.get(hz, "the replay disagrees with alpha_equal")
        print("    %-11s %4d of %d   %s" % (hz, len(rows), len(g1), label))
        if not quiet:
            for k, w in rows[:12]:
                print("                            %s  [%s]" % (k[:56], w))
    #
    # SAVECLASS IS NOT COUNTED AGAINST A SYMBOL.  On its own it is a
    # precondition and not a defect -- a callee-saved register used as scratch
    # inside one live range with no call in it is interchangeable with a
    # caller-saved one.  CALLLIVE is the same shape once the value has to
    # survive something, and that one counts.
    #
    REAL = ("RET", "CALLLIVE", "LOOPREBIND", "REPLAY")
    dirty = {r[0] for hz in REAL for r in tally.get(hz, [])}
    clean = [k for k in g1 if k not in dirty]
    print("\n  CLEAN of every shape that is not vacuous : %d of %d"
          % (len(clean), len(g1)))
    print("  (SAVECLASS alone does not count against a symbol; see the code)")
    print("\n  A clean symbol is grade 1 AND carries none of the four shapes")
    print("  where the renaming argument fails to close.  For those the")
    print("  argument IS a proof: the two instruction streams differ in")
    print("  nothing but register NAMES, the naming is one bijection over")
    print("  each live range, %esp and %ebp are pinned so every stack address")
    print("  agrees, every immediate and displacement is compared literally,")
    print("  relocated operands are compared by TARGET, and %st(N) stack")
    print("  indices survive the register substitution and are compared")
    print("  literally too -- so the two compute the same function on every")
    print("  input, by construction and not by testing.")
    return 0


# ------------------------------------------------------ the evidence map

BUILD = os.environ.get("EQPROOF_BUILD", os.path.join(ROOT, "build"))
SNAPSHOT = os.path.join(ROOT, "test", "mutations", "snapshot.json")
SUITES = os.path.join(ROOT, "test", "mutations", "suites.json")


def direct_map():
    """{symbol: {test binary, ...}} for every `ref_SYMBOL` a test REFERENCES.

    NOT BY GREPPING THE SOURCES.  Every test file opens with a block of
    `extern ref_*` declarations, so a grep counts a symbol as driven the moment
    it is DECLARED -- including one whose calls were deleted in some earlier
    revision.  An undefined symbol in the compiled object means the compiler
    emitted a reference, which only a call or an address can do.  That is
    `coverage.py`'s rule and the reason it is right.
    """
    out = {}
    objs = sorted(glob.glob(os.path.join(BUILD, "test", "unit", "*.o")))
    if not objs:
        sys.exit("eqproof.py: no test objects in %s/test/unit -- `make phase` "
                 "has not run here, and every class below would be computed "
                 "against nothing.  F2400, F3100." % BUILD)
    for o in objs:
        name = os.path.basename(o)[:-2]
        txt = subprocess.run(["nm", "-u", o], capture_output=True,
                             text=True).stdout
        for line in txt.splitlines():
            f = line.split()
            if len(f) == 2 and f[0] == "U" and f[1].startswith("ref_"):
                out.setdefault(f[1][4:], set()).add(name)
    return out, len(objs)


CALLREL = re.compile(r"^\s*([0-9a-f]+):\s+(R_386_\w+)\s+(\S+)")


def our_callgraph():
    """{caller: {callee, ...}} over the objects the differential tier LINKS.

    An edge is any relocation out of a function's body, so an address stored in
    a table counts as well as a `call`.  That over-approximates, and the
    direction matters: a spurious edge moves a symbol from NONE into
    COMPOSITE, which reports its evidence as slightly stronger than it is.  A
    MISSING edge would do the opposite and put a symbol with real evidence into
    NONE.  Both classes are weak and the report says so; over-approximating is
    the choice that does not invent an absence.
    """
    cache = os.path.join(BUILD, "eqproof_graph.json")
    objs = sorted(glob.glob(os.path.join(BUILD, "repro", "**", "*.o"),
                            recursive=True))
    if not objs:
        sys.exit("eqproof.py: no objects under %s/repro -- run `make phase` "
                 "first.  F3055." % BUILD)
    newest = max(os.path.getmtime(o) for o in objs)
    if os.path.exists(cache) and os.path.getmtime(cache) >= newest:
        with open(cache) as f:
            return {k: set(v) for k, v in json.load(f).items()}, len(objs)
    g = {}
    for o in objs:
        txt = subprocess.run(["objdump", "-dr", "--no-show-raw-insn", o],
                             capture_output=True, text=True).stdout
        cur = None
        for line in txt.splitlines():
            m = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
            if m:
                cur = m.group(1)
                g.setdefault(cur, set())
                continue
            r = CALLREL.match(line)
            if r and cur:
                t = r.group(3).split("+")[0].split("-")[0]
                if t and not t.startswith("."):
                    g[cur].add(t)
    try:
        os.makedirs(BUILD, exist_ok=True)
        with open(cache, "w") as f:
            json.dump({k: sorted(v) for k, v in g.items()}, f)
    except OSError:
        pass
    return g, len(objs)


def reachable(graph, roots):
    seen, work = set(), list(roots)
    while work:
        n = work.pop()
        for m in graph.get(n, ()):
            if m not in seen:
                seen.add(m)
                work.append(m)
    return seen


def check_counts():
    """{binary: total checks it reported}, or None if nobody measured them.

    The harness prints `PASS <section> <n> checks` per section and NEVER names
    its own binary, so the counts cannot be recovered from a `make phase` log
    under -jN -- three binaries' sections interleave and no line says whose is
    whose.  `tools/eqproof-checks.sh` runs each binary alone and writes the
    file.  A MISSING FILE IS REPORTED AS MISSING and never as zero.
    """
    path = os.environ.get("EQPROOF_CHECKS",
                          os.path.join(BUILD, "eqproof_checks.txt"))
    if not os.path.exists(path):
        return None
    out = {}
    for line in open(path):
        f = line.split()
        if len(f) == 2 and f[1].isdigit():
            out[f[0]] = int(f[1])
    return out or None


def mutation_map():
    """{binary: (caught, total)} from the recorded snapshot, and the count.

    THE SNAPSHOT IS READ, NOT RE-RUN.  `mutsnap.py` keys each suite on
    everything that can reach its binary and says for itself whether the entry
    is still valid; re-running 210 suites to learn what is already written down
    is the cost that file exists to avoid.
    """
    if not (os.path.exists(SNAPSHOT) and os.path.exists(SUITES)):
        return None, 0
    with open(SUITES) as f:
        suites = {k: v for k, v in json.load(f).items() if not k.startswith("_")}
    with open(SNAPSHOT) as f:
        snap = json.load(f).get("suites", {})
    #
    # THE METRIC IS `NOT caught`, NOT `caught == total`.  266 of the 9,252
    # recorded mutations carry `"equivalent": true` -- they are EXPECTED to
    # survive, because the change provably cannot alter behaviour, and their
    # survival is the recorded result.  Scoring `caught < total` as a weakness
    # marked 105 of 210 suites as imperfect when the real number of uncaught
    # mutations is 146 spread over far fewer.  An equivalent mutation counted
    # against a test is the test being blamed for being right.
    #
    out = {}
    pat = re.compile(r"(\d+) mutations: (\d+) caught .*?(\d+) NOT caught, "
                     r"(\d+) unusable, (\d+) equivalent")
    unparsed = 0
    for name, ent in snap.items():
        if name not in suites:
            continue
        binary = os.path.basename(suites[name][1])
        m = pat.match(ent.get("summary", ""))
        if not m:
            unparsed += 1
            continue
        tot, caught, notc = int(m.group(1)), int(m.group(2)), int(m.group(3))
        a, b, c = out.get(binary, (0, 0, 0))
        out[binary] = (a + caught, b + tot, c + notc)
    if unparsed:
        print("  eqproof: %d snapshot summary/summaries did not parse and are "
              "NOT counted below" % unparsed, file=sys.stderr)
    return out, len(snap)


EXHAUSTIVE = re.compile(r"exhaustiv", re.I)


def exhaustive_claims():
    """({binary: [section labels claiming it]}, {binary: prose claim}).

    A CLAIM, HARVESTED -- never a measurement, and the two kinds are kept
    apart because they are worth different amounts.

      a `diff_begin` LABEL is scoped to one section, so it says which sweep is
      exhaustive.  `t_fpm_phasordp` opens
      "FPM_phasor_dp exhaustive over phase, fraction at rest" and its header
      says in the same breath that `frac_phase x frac_inc` is 2^32 pairs and
      is NOT swept -- the label is exact about which half it means.

      a mention in PROSE is a claim about the file and cannot be attributed to
      a sweep at all.  Counted, reported separately, and never added to the
      first number.

    Nothing in this tree can compute a function's input domain, so neither
    kind is evidence on its own: both are pointers to a paragraph a reader has
    to go and read.  Inferring exhaustiveness from a check count would be the
    dead detector this tree keeps rediscovering.
    """
    labels, prose = {}, {}
    for src in sorted(glob.glob(os.path.join(ROOT, "test", "unit", "*.c"))
                      + glob.glob(os.path.join(ROOT, "test", "unit", "*.cpp"))):
        name = os.path.basename(src).rsplit(".", 1)[0]
        try:
            txt = open(src, errors="replace").read()
        except OSError:
            continue
        hits = [m.group(1) for m in
                re.finditer(r'diff_begin\s*\(\s*"([^"]*)"', txt)
                if EXHAUSTIVE.search(m.group(1))]
        if hits:
            labels[name] = hits
        elif EXHAUSTIVE.search(txt):
            for line in txt.splitlines():
                if EXHAUSTIVE.search(line):
                    prose[name] = line.strip().lstrip("*/ ").strip()
                    break
    return labels, prose


#
# THE FIVE LABEL CLAIMS, READ.  This is a HAND result and it is data, not
# something this tool computes: each `diff_begin` label claiming exhaustiveness
# was opened and the sweep under it read, in this worktree, at the commit the
# accompanying finding names.
#
# WHAT THE READING FOUND IS THE POINT.  Two of the five sweep an entire input
# domain and are proofs.  Three sweep a REDUCED one -- all 65536 phases against
# 14 chosen increments of 65536; all 3^n sequences over an alphabet of 3 shorts
# out of 65536 -- and every one of the three says so in its own prose, honestly
# and in detail.  **None of the five can be told from its LABEL**, which is why
# the count of symbols covered by a claim must never be read as a count of
# proofs.  The distinction is structural: the label names a sweep, and whether
# a sweep is the whole domain is a fact about the function's signature that no
# label carries.
#
VERIFIED_CLAIMS = {
    "t_v8util":      ("WHOLE",
                      "256 of 256 values of one unsigned char"),
    "t_fpm_div":     ("WHOLE",
                      "65536 of 65536 values of one unsigned short"),
    "t_fpm_phasor":  ("REDUCED",
                      "all 65536 phases x 14 chosen increments of 65536"),
    "t_fpm_phasordp": ("REDUCED",
                       "exhaustive over phase; frac_phase x frac_inc is 2^32 "
                       "and is expressly NOT swept -- its header says so"),
    "t_v22prc":      ("REDUCED",
                      "all 3^n sequences over an alphabet of 3 shorts of "
                      "65536; the argument for the 3 is in the comment"),
}

PTR = re.compile(r"[*&]")


def scalar_only(sym):
    """Does every parameter fit in a register?  True, False or None.

    THE AFFORDABILITY BOUNDARY FOR AN EXHAUSTIVE SWEEP IS THE SIGNATURE, and
    it is sharp rather than gradual.  A function of one `unsigned short` has a
    domain of 65536 and `t_fpm_div` sweeps all of it in a fraction of a second.
    A function taking a `struct *` has no input domain at all -- it has a
    reachable STATE SPACE, whose size is a fact about the whole program -- and
    no sweep can be exhaustive over that.  So this does not estimate a cost; it
    says which side of the boundary a symbol is on.

    `None` where our source does not settle it, and `None` is never counted as
    either side.
    """
    d = _demangled(sym)
    if "(" in d and sym.startswith("_Z"):
        args = d[d.index("(") + 1:d.rindex(")")]
        if args.strip() in ("", "void"):
            return True
        return not PTR.search(args)
    #
    # A PLAIN C NAME CARRIES NO SIGNATURE AT ALL, so the declaration has to be
    # found.  Anything ambiguous stays None rather than guessing -- a wrong
    # answer here would inflate the population an exhaustive sweep is being
    # called affordable for.
    #
    #
    # `next(iter(...))`, NOT `.pop()`.  `got` IS the cached set, and `.pop()`
    # EMPTIES IT -- so the first call for a symbol answered and every call
    # after it returned None.  `classify` asks three times, once per class, and
    # the three lists therefore stopped partitioning: 201 + 130 + 349 = 680
    # over a population of 666.  The columns not adding up is what gave it
    # away, and a report whose columns are never summed would have carried it.
    #
    got = _arglists().get(sym)
    return next(iter(got)) if got and len(got) == 1 else None


ARGDECL = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
TYPEISH = re.compile(r"\b(void|char|short|int|long|float|double|signed|"
                     r"unsigned|struct|union|enum|const|size_t|bool|"
                     r"[A-Za-z_][A-Za-z0-9_]*_t)\b")


def _arglists():
    """{name: {True/False}} -- does every declaration of `name` take scalars?

    BUILT ONCE OVER THE WHOLE TREE.  The first version searched every line of
    every source for one symbol at a time, which is 667 walks of 300,000 lines
    for one walk's worth of information and turned a two-second report into
    minutes.  Same data, indexed the other way round.
    """
    if "args" in _DECL_CACHE:
        return _DECL_CACHE["args"]
    idx = {}
    for f, ls in _files():
        for line in ls:
            st = line.strip()
            if "(" not in st or st.startswith(("//", "*", "/*", "#")):
                continue
            for m in ARGDECL.finditer(st):
                name = m.group(1)
                if name in ("if", "while", "for", "switch", "return", "sizeof",
                            "defined", "assert"):
                    continue
                j = m.end() - 1
                depth, k = 0, j
                while k < len(st):
                    if st[k] == "(":
                        depth += 1
                    elif st[k] == ")":
                        depth -= 1
                        if depth == 0:
                            break
                    k += 1
                if k >= len(st):
                    continue
                #
                # A DECLARATION, NOT A CALL.  A call site's arguments are
                # expressions, so reading `FPM_div(x, &r, &s)` out of its own
                # test scores a function of one `unsigned short` as taking
                # pointers.
                #
                # THE TEST IS THE ARGUMENT LIST AND NOT A LEADING RETURN TYPE.
                # Requiring a type before the name looks right and throws away
                # most of this tree: the house style puts the return type on
                # its OWN LINE and the name at column 0, so
                # `FPM_phasor(struct fpm_phasor *p);` carries no type on the
                # line the name is on.  That rule left 349 of 666 unresolved --
                # 52.4%, which is not a measurement.  A parameter list, by
                # contrast, always names types.
                #
                args = st[j + 1:k].strip()
                if args not in ("", "void") and not TYPEISH.search(args):
                    continue
                idx.setdefault(name, set()).add(
                    True if args in ("", "void") else not bool(PTR.search(args)))
    _DECL_CACHE["args"] = idx
    return idx


def classify(quiet=False, only=None, sym=None):
    g0, g1, rest, objs = grade_partition()
    pop = set(rest)
    n = len(g0) + len(g1) + len(rest)
    direct, ntestobj = direct_map()
    graph, nobj = our_callgraph()
    checks = check_counts()
    muts, nsuites = mutation_map()
    labels, prose = exhaustive_claims()

    #
    # THE ROOTS ARE EVERY DIRECTLY-DRIVEN SYMBOL, whatever its grade.  A
    # grade-0 function a test drives still reaches its callees, and leaving it
    # out because it happens to be byte-identical would put those callees in
    # NONE for a reason that has nothing to do with them.
    #
    roots = sorted(set(direct))
    reach = reachable(graph, roots)

    D = sorted(k for k in pop if k in direct)
    C = sorted(k for k in pop if k not in direct and k in reach)
    N = sorted(k for k in pop if k not in direct and k not in reach)
    assert len(D) + len(C) + len(N) == len(pop)

    if sym:
        return _one(sym, g0, g1, pop, direct, reach, checks, muts, labels)

    def best(k):
        if not checks:
            return -1
        return max((checks.get(t, -1) for t in direct.get(k, ())), default=-1)

    #
    # THE TAXONOMY, strongest first.  A symbol lands in the FIRST class it
    # qualifies for, so the classes partition and the column adds up.
    #
    #
    # A PROSE MENTION IS NOT A RUNG ON THIS LADDER, and putting it on one was
    # wrong in the direction that flatters.  The fallback matched any line of
    # any test source containing "exhaustiv" -- `t_agc`'s "exhaustive search
    # over the denormal range", `t_dialer`'s "that is swept exhaustively
    # below" -- and then credited every symbol that binary drives.  One grep
    # hit became 110 of 674, ranked ABOVE sampled, on the strength of a comment
    # about one sweep inside a binary driving twenty symbols.  The prose
    # mentions are still listed, as pointers to paragraphs worth reading, and
    # they are counted nowhere.
    #
    #
    # AND THE LABEL MUST NAME THE SYMBOL.  Crediting every symbol a claiming
    # BINARY drives put 15 symbols in the whole-domain class off the back of
    # two sweeps -- `t_v8util` sweeps `charFlip` over all 256 bytes and drives
    # fourteen other v8 utilities that it does not sweep at all.  That is the
    # per-section-against-per-symbol error made in the one place where it
    # changes the headline, and a warning printed underneath does not repair a
    # number.  Every claiming label in this tree happens to name its own
    # function -- "FPM_div exhaustive", "charFlip: bit reversal, exhaustive" --
    # so the attribution is available and there is no excuse for the looser one.
    #
    #
    # A LABEL MAY NAME A FAMILY.  `t_v22prc` sweeps `RxTrained1200` and
    # `RxTrained2400` together under "RxTrained exhaustive, n = 0..8", and an
    # exact-name rule drops both -- an under-count, which is the safe direction
    # but a wrong one when the fix is available.  So a label WORD also matches
    # when it is a prefix of the symbol's own name AND is at least eight
    # characters long.  The length floor is what stops a label word like
    # "process" or "reset" sweeping in every class that has one; eight is long
    # enough that a prefix is a deliberate family name and not a coincidence.
    #
    def claims_for(k):
        short = _demangled(k).split("(")[0].split("::")[-1].split("<")[0].strip()
        out = []
        for t in direct.get(k, ()):
            for lab in labels.get(t, ()):
                hit = re.search(r"(^|[^A-Za-z0-9_])%s([^A-Za-z0-9_]|$)"
                                % re.escape(short), lab)
                if not hit:
                    hit = any(len(w) >= 8 and short.startswith(w)
                              for w in re.findall(r"[A-Za-z_][A-Za-z0-9_]*", lab))
                if hit:
                    out.append((t, lab))
        return out

    ex_label = [k for k in D if claims_for(k)]
    whole = [k for k in ex_label
             if any(VERIFIED_CLAIMS.get(t, ("", ""))[0] == "WHOLE"
                    for t, _ in claims_for(k))]
    reduced = [k for k in ex_label if k not in whole]
    samp = [k for k in D if k not in set(ex_label) and best(k) >= 100]
    adhoc = [k for k in D if k not in set(ex_label) and 0 <= best(k) < 100]
    unkn = [k for k in D if k not in set(ex_label) and best(k) < 0]

    print("  blob    : %s" % BI.BLOB)
    print("  ours    : %s" % BI.OURS)
    print("  linked  : %s/repro, %d object(s), %d call edge(s)"
          % (BUILD, nobj, sum(len(v) for v in graph.values())))
    print("  tests   : %d compiled test object(s), %d mutation suite(s) recorded"
          % (ntestobj, nsuites))
    print("")
    print("What our differential evidence PROVES, over the %d symbol(s) the blob"
          "\nand our period build both define -- byteident.py's denominator "
          "exactly.\n" % n)
    print("  grade 0  byte-identical, so the question does not arise  : %4d  (%.1f%%)"
          % (len(g0), 100.0 * len(g0) / n))
    print("  grade 1  proven by CONSTRUCTION -- see --grade1          : %4d  (%.1f%%)"
          % (len(g1), 100.0 * len(g1) / n))
    print("  -------------------------------------------------------------------")
    print("  NEITHER, so each needs a coverage argument               : %4d  (%.1f%%)"
          % (len(pop), 100.0 * len(pop) / n))
    print("")
    print("  I. WHERE THE EVIDENCE COMES FROM, over those %d\n" % len(pop))
    for lab, rows, why in (
            ("DIRECT", D, "a test names its `ref_` alias, so it is compared"),
            ("COMPOSITE", C, "no test names it; our build reaches it from one"),
            ("NONE", N, "neither.  No differential evidence of any kind")):
        print("    %-10s %4d  (%5.1f%%)   %s"
              % (lab, len(rows), 100.0 * len(rows) / max(1, len(pop)), why))
    print("                                   DIRECT: at its own boundary, on")
    print("                                   inputs the test chose.  COMPOSITE:")
    print("                                   only through a caller, on inputs")
    print("                                   that caller's test induces, and")
    print("                                   only as far as its output reveals.")
    print("")
    print("  II. WHAT THAT EVIDENCE PROVES, strongest first.  A symbol lands in")
    print("      the FIRST class it qualifies for, so these partition the %d.\n"
          % len(pop))
    rows = [
        ("EXHAUSTIVE, whole", whole,
         "read: the sweep covers the entire input domain"),
        ("EXHAUSTIVE, reduced", reduced,
         "read: a reduced domain, with the argument written"),
        ("SAMPLED", samp,
         "driven, >=100 checks, no exhaustiveness claimed"),
        ("AD-HOC VECTORS", adhoc,
         "driven on fewer than 100 checks"),
        ("UNMEASURED", unkn,
         "driven, but no check count was recorded"),
        ("UNTESTED", C + N,
         "COMPOSITE or NONE: nothing compares it directly"),
    ]
    for lab, r, why in rows:
        print("    %-19s %4d  (%5.1f%%)  %s"
              % (lab, len(r), 100.0 * len(r) / max(1, len(pop)), why))
    print("")
    print("    THE FIRST TWO ARE THE ONLY CLASSES THAT COULD BE PROOFS, and the")
    print("    split between them was made BY READING all %d claiming label(s)"
          % len(labels))
    print("    -- it is not computed.  None of the five could be told apart from")
    print("    its label alone, and that is structural: a label names a sweep,")
    print("    and whether a sweep is the whole domain is a fact about the")
    print("    signature that no label carries.")
    print("")
    print("    A CLAIM IS COUNTED ONLY WHERE ITS LABEL NAMES THE SYMBOL, so")
    print("    this is per symbol and not per binary.  What it still cannot say")
    print("    is whether the labelled sweep covers all of THAT symbol's inputs:")
    print("    `FPM_phasor exhaustive` sweeps every phase against fourteen")
    print("    chosen increments, and is exhaustive in one argument of two.")
    print("    So %d is an upper bound on the proofs, not a count of them --"
          % len(ex_label))
    print("    which is what the WHOLE/REDUCED split above is for.")
    print("")
    print("    %d further test source(s) mention exhaustiveness in PROSE, and"
          % len(prose))
    print("    are listed below and counted NOWHERE.  Such a mention is about")
    print("    one sweep inside a binary driving many symbols; crediting them")
    print("    all put 110 of %d above SAMPLED on the strength of a comment."
          % len(pop))
    print("")
    if checks is None:
        print("  III. HOW MANY INPUTS: NOT MEASURED, and said so rather than")
        print("       printed as zero.  The harness never names its own binary,")
        print("       so a -jN phase log cannot be attributed to one.  Run")
        print("       `sh tools/eqproof-checks.sh`.")
    else:
        print("  III. HOW MANY INPUTS.  The %d DIRECT symbols banded by the check"
              % len(D))
        print("       count of the LARGEST binary driving each.  Per BINARY, never")
        print("       per symbol: one binary may drive twenty and this credits its")
        print("       whole total to each, so every band is an UPPER bound.\n")
        band = [("under 100", 0, 100), ("100 - 999", 100, 1000),
                ("1k - 99k", 1000, 100000), ("100k and up", 100000, 1 << 62)]
        for lab, lo, hi in band:
            c = len([k for k in D if lo <= best(k) < hi])
            print("       %-13s %4d  (%5.1f%%)"
                  % (lab, c, 100.0 * c / max(1, len(D))))
        tot = sum(checks.values())
        print("\n       %d checks over %d binaries in all."
              % (tot, len(checks)))
    print("")
    if muts is None:
        print("  IV. DISCRIMINATING POWER: NO SNAPSHOT at %s" % SNAPSHOT)
    else:
        ws = [k for k in D if any(t in muts for t in direct[k])]
        ac = [k for k in ws
              if all(muts[t][2] == 0 for t in direct[k] if t in muts)]
        nm = sum(muts[t][1] for t in muts)
        nc = sum(muts[t][2] for t in muts)
        print("  IV. DISCRIMINATING POWER, which is the only thing here that")
        print("      speaks to whether a test could TELL.  F8163: a non-vacuity")
        print("      guard proves a branch was REACHED, not that the test can")
        print("      tell it from its alternative.  `mutate.py` judges exactly")
        print("      that, and its verdicts are recorded.\n")
        print("      a mutation suite covers a binary that drives it : %4d of %d"
              "  (%.1f%%)" % (len(ws), len(D), 100.0 * len(ws) / max(1, len(D))))
        print("      ...and NOTHING in those suites went uncaught    : %4d of %d"
              "  (%.1f%%)" % (len(ac), len(D), 100.0 * len(ac) / max(1, len(D))))
        print("      no mutation suite reaches it at all            : %4d of %d"
              "  (%.1f%%)" % (len(D) - len(ws), len(D),
                              100.0 * (len(D) - len(ws)) / max(1, len(D))))
        print("\n      %d mutations recorded over %d suite(s); %d went "
              "UNCAUGHT.\n      An uncaught mutation is an untested claim -- "
              "the test cannot\n      tell that spelling of the code from the "
              "one we shipped." % (nm, nsuites, nc))
    #
    # WHAT IT WOULD COST TO MOVE THE WEAK CLASSES UP.  Not an estimate -- a
    # boundary, and the boundary is the INPUT DOMAIN.  A signature of scalars
    # has one a loop can enumerate.  A `struct *` read as state does not: it
    # has a reachable STATE SPACE, which is a fact about the whole program.
    #
    # A POINTER PARAMETER IS NOT AUTOMATICALLY AN INPUT, and this tree's own
    # worked example says so.  `FPM_div(unsigned short, unsigned short *,
    # unsigned short *)` takes two pointers and `t_fpm_div` still sweeps its
    # WHOLE domain in 65,536 trials -- both pointers are OUT parameters and the
    # input is the one short.  So the count below splits on what can be read
    # off a signature and no further: no pointer at all is a definite yes, and
    # a pointer is a "read this one", not a no.
    #
    sc = [k for k in samp if scalar_only(k) is True]
    pt = [k for k in samp if scalar_only(k) is False]
    un = [k for k in samp if scalar_only(k) is None]
    #
    # THE COLUMNS MUST ADD UP, AND SAYING SO IS NOT DECORATION.  These three
    # are answers to one three-valued question over one population, so their
    # total is that population by construction -- and when `scalar_only` was
    # emptying its own cache the total came to 680 over 666 and the report
    # printed it without complaint.  A count that cannot be wrong is worth
    # asserting precisely because nobody re-adds a column by hand.
    #
    assert len(sc) + len(pt) + len(un) == len(samp), (
        "eqproof: %d + %d + %d != %d -- the scalar/pointer/unknown columns do "
        "not partition SAMPLED, so at least one of them is not measuring what "
        "its label says" % (len(sc), len(pt), len(un), len(samp)))
    print("\n  V. WHAT IT WOULD COST TO MOVE `SAMPLED` UP.  Not an estimate: a")
    print("     boundary, and it is the INPUT DOMAIN rather than the parameter")
    print("     list.  Of the %d SAMPLED, by our own declarations:\n" % len(samp))
    print("       no pointer parameter at all         : %4d  (%5.1f%%)"
          % (len(sc), 100.0 * len(sc) / max(1, len(samp))))
    print("                     the domain IS the parameters, so a sweep is")
    print("                     conceivable and its cost is their total width")
    print("       at least one pointer parameter      : %4d  (%5.1f%%)"
          % (len(pt), 100.0 * len(pt) / max(1, len(samp))))
    print("                     READ IT.  A pointer may be an OUT parameter and")
    print("                     cost the domain nothing -- `FPM_div` takes two")
    print("                     and is swept over all 65536 of its one input --")
    print("                     or it may be state, and then no sweep exhausts it")
    print("       our source does not settle it       : %4d  (%5.1f%%)"
          % (len(un), 100.0 * len(un) / max(1, len(samp))))
    print("\n     AND CONCEIVABLE IS NOT AFFORDABLE: the width decides.  One")
    print("     16-bit argument is 65,536 trials and `t_fpm_div` runs it in")
    print("     under a second.  Two is 2^32, and `t_fpm_phasordp` declined")
    print("     exactly that -- it swept the carry boundary instead and wrote")
    print("     down which half it had not covered.  So the ceiling on \"make it")
    print("     exhaustive\" is a SUBSET of %d, against a population of %d."
          % (len(sc), len(pop)))
    print("\n     THE CHEAP AXIS IS THE OTHER ONE.  %d DIRECT symbol(s) are"
          % (len(D) - len([k for k in D if any(t in (muts or {})
                                              for t in direct[k])])))
    print("     reached by no mutation suite, and a mutation suite needs no new")
    print("     input domain: it asks whether the test could TELL, which is")
    print("     exactly the question a sample leaves open.  It is the only axis")
    print("     here that moves at a cost this tree already pays.")
    if not quiet and (labels or prose):
        print("\n  the exhaustiveness claims, to be read and not counted:\n")
        for c in sorted(labels):
            print("      %-20s %s" % (c, labels[c][0][:50]))
        for c in sorted(labels):
            v = VERIFIED_CLAIMS.get(c)
            if v:
                print("        %-9s read: %s" % (v[0], v[1][:56]))
        print("\n  mentions in PROSE -- pointers, counted nowhere:\n")
        for c in sorted(prose):
            print("      %-20s %s" % (c, prose[c][:44]))
    if only:
        rows = {"DIRECT": D, "COMPOSITE": C, "NONE": N,
                "EXHAUSTIVE": ex_label, "WHOLE": whole, "REDUCED": reduced,
                "SAMPLED": samp, "ADHOC": adhoc,
                "SCALAR": sc, "POINTER": pt}.get(only.upper())
        if rows is None:
            sys.exit("eqproof.py: --class wants DIRECT, COMPOSITE, NONE, "
                     "EXHAUSTIVE, WHOLE, REDUCED, SAMPLED, ADHOC, SCALAR or "
                     "POINTER")
        print("\n  %s -- %d symbol(s):\n" % (only.upper(), len(rows)))
        for k in rows:
            ts = sorted(direct.get(k, ()))
            c = best(k)
            print("    %-58s %s%s"
                  % (k[:58], ",".join(ts[:2]),
                     "  %d checks" % c if c >= 0 else ""))
    return 0


def _one(sym, g0, g1, pop, direct, reach, checks, muts, claims):
    print("  %s\n" % sym)
    print("  demangled : %s" % _demangled(sym))
    if sym in g0:
        grade = "0 -- byte-identical"
    elif sym in g1:
        grade = "1 -- same instructions under a per-live-range renaming"
    elif sym in pop:
        grade = "neither -- this is the population that needs evidence"
    else:
        grade = ("NOT in byteident's denominator: the blob and our period "
                 "build do not both define it")
    print("  grade     : %s" % grade)
    if sym in direct:
        ts = sorted(direct[sym])
        print("  evidence  : DIRECT -- %d binary/binaries name ref_%s"
              % (len(ts), sym))
        for t in ts:
            bits = []
            if checks and t in checks:
                bits.append("%d checks" % checks[t])
            if muts and t in muts:
                bits.append("%d/%d mutations caught, %d NOT"
                            % (muts[t][0], muts[t][1], muts[t][2]))
            for lab in claims.get(t, ()):
                bits.append("label claims exhaustive: %s" % lab)
            print("                %-24s %s" % (t, "; ".join(bits) or "-"))
    elif sym in reach:
        print("  evidence  : COMPOSITE -- no test names ref_%s, and our build "
              "reaches\n              it from one that is driven" % sym)
    else:
        print("  evidence  : NONE -- no test names it, and nothing a test "
              "drives\n              reaches it in our build")
    return 0


# --------------------------------------------------------------- selftest

def _rows(text):
    """Rows from a tiny assembly listing written as `mnemonic<space>operands`."""
    out = []
    for line in text.strip().splitlines():
        p = line.strip().split(None, 1)
        out.append((p[0], p[1].strip() if len(p) > 1 else ""))
    return out


SELFTEST = [
    #
    # EVERY CASE IS A PAIR `alpha_equal` ACCEPTS.  That is the whole point: a
    # hazard this tool reports on a pair byteident REJECTS would be no news.
    # The first case must be clean and the other four must fire, one shape
    # each -- a probe that cannot fail is not a probe (F3111), and a probe
    # that fires on everything is not one either.
    #
    ("clean swap, nothing escapes", set(), """
        mov 0x8(%esp),%ebx
        mov 0x4(%esp),%esi
        add %esi,%ebx
        mov %ebx,0xc(%esp)
        ret
     ""","""
        mov 0x8(%esp),%esi
        mov 0x4(%esp),%ebx
        add %ebx,%esi
        mov %esi,0xc(%esp)
        ret
     """),
    ("the return value is in the wrong register", {"RET"}, """
        mov 0x4(%esp),%eax
        ret
     ""","""
        mov 0x4(%esp),%edx
        ret
     """),
    ("a callee-saved value is held in a caller-saved register",
     {"SAVECLASS"}, """
        push %ebx
        mov 0x8(%esp),%ebx
        add $0x1,%ebx
        mov %ebx,0xc(%esp)
        pop %ebx
        ret
     ""","""
        push %ecx
        mov 0x8(%esp),%ecx
        add $0x1,%ecx
        mov %ecx,0xc(%esp)
        pop %ecx
        ret
     """),
    ("...and a call stands in the middle of it",
     {"SAVECLASS", "CALLLIVE"}, """
        push %ebx
        mov 0x8(%esp),%ebx
        call .+0x40
        mov %ebx,0xc(%esp)
        pop %ebx
        ret
     ""","""
        push %ecx
        mov 0x8(%esp),%ecx
        call .+0x40
        mov %ecx,0xc(%esp)
        pop %ecx
        ret
     """),
]

#
# THE LOOP CONTROL.  Both sides accumulate into a CALLEE-SAVED register and
# neither leaves anything in %eax, so neither RET nor SAVECLASS can fire and
# LOOPREBIND is the only shape left.  A probe that fires because some OTHER
# probe fired has not been shown to work.
#
# The branch target is spelt `.+3`, not `.+0x3`: `byteident.insns` rewrites a
# bare hex target as `.%+d`, in DECIMAL.  The first draft wrote hex, the span
# regex matched nothing, no back edge was found, and the control "passed"
# reporting the RET hazard from a different mistake in the same listing.
#
# What the two do: the blob carries the running sum in %ebx across the back
# edge.  Ours puts 9 in %edi instead, so ITS %ebx still holds the sum on the
# second pass where the blob's holds 9.  They agree on one pass and diverge on
# the next -- which is exactly what a linear scan cannot see.
#
LOOP_BLOB = """
        mov $0x0,%ebx
        add (%esi),%ebx
        mov %ebx,(%edi)
        mov $0x9,%ebx
        jne .+3
        ret
"""
LOOP_OURS = """
        mov $0x0,%ebx
        add (%esi),%ebx
        mov %ebx,(%edi)
        mov $0x9,%edi
        jne .+3
        ret
"""


def selftest():
    fails = 0
    print("eqproof --selftest\n")
    print("  the four hazard shapes, on pairs `alpha_equal` ACCEPTS:\n")
    for name, want, a, b in SELFTEST:
        x, y = _rows(a), _rows(b)
        accepted = BI.alpha_equal(x, y)
        got = set(hazards(x, y))
        ok = accepted and got == want
        fails += 0 if ok else 1
        print("    %-4s %-52s alpha_equal=%-6s hazards=%s"
              % ("ok" if ok else "FAIL", name, "ACCEPT" if accepted else
                 "REJECT", ",".join(sorted(got)) or "(none)"))
        if not accepted:
            print("         the control is broken: byteident REJECTS this "
                  "pair, so any hazard here is not news")
    #
    # THE LOOP CASE.  Offsets are needed for a back edge to exist at all, so
    # this one is fed rows with hand-written offsets rather than through
    # `rows_with_offsets`.  The blob accumulates into %eax across the back
    # edge; ours puts the same value in %ecx and its %eax still holds the old
    # accumulator, so the two agree on the first pass and diverge on the
    # second -- which a linear scan cannot see.
    #
    x, y = _rows(LOOP_BLOB), _rows(LOOP_OURS)
    rw = [(m, o, i * 3) for i, (m, o) in enumerate(x)]
    accepted = BI.alpha_equal(x, y)
    got = set(hazards(x, y, rw))
    ok = accepted and got == {"LOOPREBIND"}
    fails += 0 if ok else 1
    print("    %-4s %-52s alpha_equal=%-6s hazards=%s"
          % ("ok" if ok else "FAIL", "a rebind across a back edge",
             "ACCEPT" if accepted else "REJECT",
             ",".join(sorted(got)) or "(none)"))
    #
    # AND THE NEGATIVE CONTROL FOR THE BACK EDGE ITSELF.  The same body with
    # no branch in it must NOT report LOOPREBIND, or the probe is reporting
    # the rebind and not the loop.
    #
    x2 = [r for r in x if r[0] != "jne"]
    y2 = [r for r in y if r[0] != "jne"]
    rw2 = [(m, o, i * 3) for i, (m, o) in enumerate(x2)]
    got2 = set(hazards(x2, y2, rw2))
    ok2 = BI.alpha_equal(x2, y2) and "LOOPREBIND" not in got2
    fails += 0 if ok2 else 1
    print("    %-4s %-52s alpha_equal=%-6s hazards=%s"
          % ("ok" if ok2 else "FAIL", "  ...the same rebind with no back edge",
             "ACCEPT" if BI.alpha_equal(x2, y2) else "REJECT",
             ",".join(sorted(got2)) or "(none)"))
    #
    # THE RETURN-TYPE LOOKUP IS A DETECTOR TOO, and it decides whether the RET
    # shape counts.  It has to be shown answering in BOTH directions on symbols
    # whose answer is known by reading, or "every RET is vacuous" would be
    # indistinguishable from a lookup that always says void.  Three of these
    # five were wrong at some point in this file's history and each was wrong
    # in the VACUOUS direction.
    #
    #
    # AND THE RE-ENTRY, which is the shape that made LOOPREBIND lie.  The cold
    # block sits at the end and jumps back into the main line -- textually a
    # backward branch across the whole body, and in the CFG a merge that runs
    # at most once.  Same rebind as the case above; it must NOT report.
    #
    reentry = """
        test %eax,%eax
        je .+15
        mov $0x0,%ebx
        add (%esi),%ebx
        mov $0x9,%ebx
        ret
        mov $0x1,%eax
        jmp .+6
    """
    reentry_ours = reentry.replace("mov $0x9,%ebx", "mov $0x9,%edi")
    x3, y3 = _rows(reentry), _rows(reentry_ours)
    rw3 = [(m, o, i * 3) for i, (m, o) in enumerate(x3)]
    got3 = set(hazards(x3, y3, rw3))
    ok3 = BI.alpha_equal(x3, y3) and "LOOPREBIND" not in got3
    fails += 0 if ok3 else 1
    print("    %-4s %-52s alpha_equal=%-6s hazards=%s"
          % ("ok" if ok3 else "FAIL",
             "  ...and a cold block jumping BACK into the main line",
             "ACCEPT" if BI.alpha_equal(x3, y3) else "REJECT",
             ",".join(sorted(got3)) or "(none)"))

    #
    # THE REACHABILITY MACHINERY, SHOWN FIRING.  `COMPOSITE` reads 0 over this
    # tree, and a class that reads zero is indistinguishable from a class that
    # cannot be reached at all -- which is finding F134 exactly, and the shape
    # that let `extcheck` print "(none)" through four broken versions.  So:
    # take a real caller/callee pair out of the graph, use ONLY the caller as a
    # root, and require the callee to come out reachable.  If this fails, the
    # zero above is this tool's and not the tree's.
    #
    print("\n  the reachability that decides COMPOSITE, shown firing:\n")
    try:
        graph, nobj = our_callgraph()
        pair = next(((a, b) for a, bs in sorted(graph.items()) for b in sorted(bs)
                     if b in graph), None)
    except SystemExit:
        graph, pair = None, None
        print("    n/a  no build to read; run `make phase` and re-run")
    if graph is not None:
        if pair is None:
            fails += 1
            print("    FAIL the call graph has no edge between two known "
                  "functions at all")
        else:
            a, b = pair
            got = b in reachable(graph, [a])
            none = b in reachable(graph, [])
            ok4 = got and not none
            fails += 0 if ok4 else 1
            print("    %-4s %s -> %s reachable from the caller: %s; from no "
                  "root at all: %s"
                  % ("ok" if ok4 else "FAIL", a[:26], b[:26], got, none))
            print("         %d function(s), %d edge(s) in the graph"
                  % (len(graph), sum(len(v) for v in graph.values())))

    print("\n  the return-type lookup, which decides whether RET counts:\n")
    for sym, want in (("_ZN5V90CPC1Ev", "VOID"),
                      ("_Z7hammingIfEvPT_j", "VOID"),
                      ("_ZN9ScramblerIhhE5resetEh", "VOID"),
                      ("FPM_ECC_free", "VOID"),
                      ("_ZN9ScramblerIhhE7processEPKhPhj", "VOID"),
                      ("_ZN9ScramblerIhhE7processEh", "VALUE"),
                      ("_ZNK19V90SpectralVerifier13freqToLeftBinEf", "VALUE")):
        got = returns_value(sym)
        ok = got == want
        fails += 0 if ok else 1
        print("    %-4s %-52s want %-7s got %s"
              % ("ok" if ok else "FAIL", _demangled(sym)[:52], want, got))
    print("\n  %d control(s), %d failed" % (len(SELFTEST) + 11, fails))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--grade1", action="store_true",
                    help="the grade-1 proof census")
    ap.add_argument("--class", dest="klass", metavar="CLASS",
                    help="list one class: DIRECT, COMPOSITE or NONE")
    ap.add_argument("--sym", metavar="NAME",
                    help="one symbol, with its grade and its evidence")
    ap.add_argument("--selftest", action="store_true",
                    help="show every probe firing on a known input")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if a.grade1:
        return grade1_census(a.quiet)
    return classify(a.quiet, a.klass, a.sym)


if __name__ == "__main__":
    sys.exit(main())
