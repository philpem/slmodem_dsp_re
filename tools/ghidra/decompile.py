# Ghidra headless script: decompile the named functions to stdout.
# Driven by tools/decompile.sh -- see that for why this exists and what the
# output is and is not good for.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import os

want = set(x for x in os.environ.get("WANT", "").split(",") if x)
di = DecompInterface()
di.openProgram(currentProgram)
for f in currentProgram.getFunctionManager().getFunctions(True):
    if want and f.getName() not in want:
        continue
    r = di.decompileFunction(f, 300, ConsoleTaskMonitor())
    print("=====BEGIN %s=====" % f.getName())
    print(r.getDecompiledFunction().getC() if r.decompileCompleted()
          else "FAILED: " + str(r.getErrorMessage()))
    print("=====END=====")
