"""
Where our own compiler's objects are, and the refusal when there are none.

WHY THIS EXISTS

Seven tools answer one question -- WHICH SYMBOLS HAS THIS TREE ALREADY
WRITTEN -- and every one of them answers it the only honest way, by reading
the symbol tables of the objects our compiler produced rather than by
grepping source for names.  Each had the directory spelled out inside it:

    glob.glob("build/src/**/*.o", recursive=True)

Task #164, "build: split the object tree so the DEFAULT build carries our
fixes", made `$(BUILD)/%.o` the FIXED build and `$(BUILD)/repro/%.o` the
faithful one, and pointed the test binaries at the second.  A plain `make`
builds the tests, so since 75dcc19 it populates `build/repro` and leaves
`build/src` EMPTY.  Nothing failed.  The seven tools went on globbing a
directory that had stopped being filled, found nothing, concluded that this
tree has written nothing, and reported that with no denominator and exit 0:

    coverage.py     translated 0.0%, 0 bytes, 0 symbols   -- exit 0
    closure.py      alaw2linear and ulaw2linear UNWRITTEN -- exit 0, and both
                    have been in src/service/pcm.c throughout
    service.py      913 unwritten symbols in data mode, "sanity checks passed"

That is finding F2400 for the third time (a build moved its output and a
detector kept reading the old path) and finding F3100 for the second, and the
answer is the one 2401 ruled and 0d30911 landed: PROBE both layouts, take
whichever HAS the objects, REFUSE on a zero denominator, and make every tool
say how many objects it read and where from.  Finding F3055 recorded it;
finding F3110 is the fix.

WHY PROBING IS SOUND HERE, AND WHY IT IS NOT A GUESS

`$(OBJ)` and `$(OBJ_REPRO)` are the SAME source list -- `find src -name
'*.c'` and `'*.cpp'` -- built twice, once with -DDSPLIB_REPRODUCE_BUGS and
once without.  The macro guards the body of a function, never its existence,
so the two trees define the same names.  MEASURED, not assumed: at cf0f543,
with both trees fully built, `nm --defined-only` over all 183 objects of each
gives 1457 (name, kind) pairs and the two sets are identical, diff empty.
So for the one question these tools ask -- does a definition of this name
exist on our side -- either tree answers it and the order below is a
preference, not a correctness argument.

`build/src` is preferred because it is what the docstrings of the tools have
always meant and what ships.  If a future split makes the two disagree, the
provenance line printed on every run is what shows which was read.

WHY THIS IS A WHITELIST OF TWO NAMED DIRECTORIES AND NOT A WALK OF build/

Finding F222: the walk used to be all of `build/` less `dsplibs_ref.o` by
name, and then the two-pass rename left `build/dsplibs_glob.o` -- the BLOB,
with all 1782 of its symbols promoted to global -- beside it.  The walk took
the blob for our own output and reported 98.0% translated for a tree that had
reconstructed 290 symbols.  Only directories our compiler writes to from
`src/` may be named here.  `build/64` is deliberately NOT one of them: it is
the C++ half alone, so it would answer the question with a true number over
the wrong denominator, which is the failure this module exists to end.
"""

import os
import sys

# Under $(BUILD), in preference order.  Both are written by our compiler from
# src/ and nothing else; see the whitelist argument above before adding a
# third.
LAYOUTS = ("src", "repro")

BUILD = "build"


def objdirs(build=BUILD):
    """Every layout this module knows about, preferred first."""
    return [os.path.join(build, d) for d in LAYOUTS]


def _objects_under(d):
    found = []
    for root, _dirs, files in os.walk(d):
        found += [os.path.join(root, f) for f in files if f.endswith(".o")]
    return sorted(found)


def objects(build=BUILD):
    """(directory, [paths]) for the first layout that has any .o at all.

    (None, []) when neither does -- which is the case the caller must refuse
    on, because an empty list and a tree that has written nothing render
    identically in every report downstream of here.
    """
    for d in objdirs(build):
        found = _objects_under(d)
        if found:
            return d, found
    return None, []


def sources(root="src"):
    """The .c and .cpp our build compiles, for the count and the mtimes."""
    found = []
    for dirpath, _dirs, files in os.walk(root):
        found += [os.path.join(dirpath, f) for f in files
                  if f.endswith((".c", ".cpp"))]
    return sorted(found)


def _newest(paths):
    t = 0.0
    for p in paths:
        try:
            t = max(t, os.path.getmtime(p))
        except OSError:
            pass
    return t


#
# THE ADVICE, AND WHY IT IS NOT `make`.
#
# `make` builds $(TESTBIN), whose prerequisites are $(OBJ_REPRO); it has not
# built $(OBJ) since #164.  Two of these tools printed "Run `make` first" and
# an agent that did exactly that got the same empty answer twice with no
# warning that anything had changed -- which is how finding F3055 was found.
# `coverage:` lists $(OBJ) as a prerequisite and `phase:` depends on
# `coverage`, so either target populates build/src.
#
ADVICE = ("`make coverage` populates build/src (it lists $(OBJ) as a\n"
          "  prerequisite); so does `make phase`, which depends on it.  A plain\n"
          "  `make` builds the tests, which link $(OBJ_REPRO), and has not\n"
          "  filled build/src since 75dcc19 (#164).")


def _tool(name=None):
    return name or os.path.basename(sys.argv[0]) or "objtree"


def refuse(what, build=BUILD, tool=None):
    """The message for a zero denominator.  Pass it straight to sys.exit().

    A DETECTOR MUST REPORT ITS DENOMINATOR (finding F2401), and zero is not a
    score.  "Nothing is written yet" and "this tool read no objects" are the
    same report in every one of these seven, and only the second is a bug, so
    the tool must not be allowed to print either.
    """
    src = sources()
    return ("%s: NO OBJECTS, so the denominator is zero and\n"
            "  %s\n"
            "  would be computed against NOTHING.  That is not a clean sheet:\n"
            "  it renders exactly as a tree that has reconstructed nothing,\n"
            "  which is why this refuses instead of reporting it.\n"
            "  Looked for **/*.o in: %s\n"
            "  %d source file(s) under src/ are waiting to be compiled.\n"
            "  %s\n"
            "  Findings F134, F2401, F3055, F3110."
            % (_tool(tool), what, ", ".join(objdirs(build)), len(src), ADVICE))


def provenance(d, objs, build=BUILD, tool=None, stream=sys.stderr):
    """One line naming the directory read and the number of objects in it.

    ON stderr, NEVER in the report body: `make coverage` and `make worklist`
    write docs/coverage.md and docs/worklist.md, which are committed, and a
    provenance line inside them would dirty the tree on every run.

    It also carries the two staleness checks, which are warnings and not
    refusals.  A partial object tree gives a wrong answer the same way an
    empty one does, but making it fatal would break `make phase` mid-build for
    anyone whose tree is half-compiled -- so it is said loudly and the human
    decides.  The count beside the source count is what makes it visible at
    all; before this, no tool of the seven printed either number.
    """
    src = sources()
    stream.write("%s: read %d object(s) from %s/ (%d source file(s) under "
                 "src/)\n" % (_tool(tool), len(objs), d, len(src)))
    if len(objs) < len(src):
        stream.write("%s: WARNING -- %d fewer object(s) than sources, so the\n"
                     "  object tree is PARTIAL and every symbol in the %d\n"
                     "  uncompiled file(s) will read as unwritten.\n"
                     "  %s\n"
                     % (_tool(tool), len(src) - len(objs),
                        len(src) - len(objs), ADVICE))
    elif _newest(src) > _newest(objs):
        stream.write("%s: WARNING -- a source under src/ is NEWER than every\n"
                     "  object in %s/, so this answer is STALE.\n"
                     "  %s\n" % (_tool(tool), d, ADVICE))
    behind(tool=tool, stream=stream)


def behind(tool=None, stream=sys.stderr):
    """Warn when this working tree is behind the integration branch.

    WHY A GIT CHECK LIVES IN THE OBJECT-TREE MODULE.  The seven tools that go
    through here all answer one question -- WHICH SYMBOLS ARE ALREADY WRITTEN
    -- and they answer it from the tree they are standing in.  An agent
    worktree branched from an old commit answers it about a project that no
    longer exists, and every symptom reads as a fact about the CODE rather
    than about the checkout: a citation resolves to nothing, a header is
    missing, a symbol looks unwritten.

    MEASURED, not hypothetical.  In the fax wave of 2026-08-31 three of four
    agent worktrees came up at `c1ca61af`, 106 commits behind, and one agent
    reported its brief's citations as "fictional" -- a true statement about
    its worktree and a false one about the tree.  The danger is not the
    confusion, it is that "is this already written?" was being answered
    against a tree missing 106 commits, which is the question whose wrong
    answer put two definitions of one symbol into a merge and failed 293 of
    293 binaries at link.

    A WARNING AND NOT A REFUSAL, for the reason the two above are: a
    legitimately detached checkout, a bisect, or a branch that has diverged on
    purpose all read as behind, and making it fatal would stop work that is
    fine.  It is said loudly and the human decides.
    """
    import subprocess

    def git(*a):
        try:
            r = subprocess.run(("git",) + a, capture_output=True, text=True)
        except OSError:
            return None
        return r.stdout.strip() if r.returncode == 0 else None

    if git("rev-parse", "--git-dir") is None:
        return
    ref = None
    for cand in ("master", "main"):
        if git("rev-parse", "--verify", "--quiet", cand):
            ref = cand
            break
    if ref is None:
        return
    n = git("rev-list", "--count", "HEAD..%s" % ref)
    if not n or not n.isdigit() or int(n) == 0:
        return
    stream.write("%s: WARNING -- this working tree is %s commit(s) behind\n"
                 "  `%s`, so \"which symbols are already written\" is being\n"
                 "  answered about an older project.  Two agents once wrote\n"
                 "  the same four symbols this way.  `git merge %s` first,\n"
                 "  then re-measure.\n" % (_tool(tool), n, ref, ref))


def read(what, build=BUILD, tool=None, stream=sys.stderr):
    """objects() + refuse() + provenance(): the whole contract in one call.

    Returns (directory, [paths]) and does not return at all when there are
    none.
    """
    d, objs = objects(build)
    if not objs:
        sys.exit(refuse(what, build, tool))
    provenance(d, objs, build, tool, stream)
    return d, objs
