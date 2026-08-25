# Ghidra headless script: decompile the named functions to stdout.
# Driven by tools/decompile.sh -- see that for why this exists and what the
# output is and is not good for.
#
# THREE WAYS TO NAME A FUNCTION, because one is not enough (finding F704).
#
#   bare        chkForceBaudRate          getName()
#   qualified   Resampler::resample       getName(True)
#   by address  0x34da0                   the blob symbol's st_value, from nm,
#                                         which is how a MANGLED name works
#
# The last is the one that matters.  Ghidra DEMANGLES, so the object's
# `_ZN9Resampler8resampleEPKfjPfRj` is stored as namespace `Resampler` plus
# name `resample` and matching the mangled string finds nothing -- while every
# C++ name written down anywhere in this project is the mangled one.  Rather
# than demangle in here, where the API moves between Ghidra versions,
# decompile.sh resolves the name through `nm` and passes the address.
#
# THE IMAGE BASE IS NOT ZERO.  Ghidra loads this relocatable object at 0x10000,
# so `nm`'s 0x34da0 is Ghidra's 0x44da0.  It is read from the program rather
# than assumed -- getImageBase() -- because nothing here should break quietly
# if a future loader picks a different base.
#
# Ambiguity is REPORTED, not silently resolved: `resample` is a real function
# on two classes and returning both is right, but the caller has to be told.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import os
import sys


def split(var):
    return [x for x in os.environ.get(var, "").split(",") if x]


want = set(split("WANT"))
want_addr = {}
base = currentProgram.getImageBase().getOffset()
for spec in split("WANT_ADDR"):
    name, _, off = spec.partition("=")
    want_addr[base + int(off, 16)] = name

di = DecompInterface()
di.openProgram(currentProgram)

matched = {}
for f in currentProgram.getFunctionManager().getFunctions(True):
    bare = f.getName()
    qual = f.getName(True)
    entry = f.getEntryPoint().getOffset()

    key = None
    if entry in want_addr:
        key = want_addr[entry]
    elif bare in want:
        key = bare
    elif qual in want:
        key = qual
    elif not want and not want_addr:
        key = bare
    if key is None:
        continue

    matched.setdefault(key, []).append(qual)
    r = di.decompileFunction(f, 300, ConsoleTaskMonitor())
    # The QUALIFIED name in the banner: two classes can share a method name,
    # and a banner that cannot tell them apart makes the output unusable for
    # anything that keys on it.
    print("=====BEGIN %s=====" % qual)
    print(r.getDecompiledFunction().getC() if r.decompileCompleted()
          else "FAILED: " + str(r.getErrorMessage()))
    print("=====END=====")

# A request that matched nothing is the failure this script used to have no
# way of expressing -- it simply printed less.  Say so, on stderr, by name.
for k in sorted(set(list(want) + list(want_addr.values()))):
    hits = matched.get(k, [])
    if not hits:
        sys.stderr.write("decompile.py: NO MATCH for %s\n" % k)
    elif len(hits) > 1:
        sys.stderr.write("decompile.py: %s is AMBIGUOUS, decompiled %d: %s\n"
                         % (k, len(hits), ", ".join(sorted(hits))))
