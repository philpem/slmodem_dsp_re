#!/bin/sh
#
# Ghidra's decompilation of one or more blob functions, as SCAFFOLDING.
#
# WHAT IT IS FOR
#
# Reading control flow and constants out of a large function without spending
# a session's worth of turns on it.  Measured against `chkForceBaudRate`,
# which this tree had already reconstructed and differentially verified by
# hand, Ghidra 11.4.2 recovered:
#
#   the branch structure, exactly
#   every constant and every structure offset (obj+0xac3c, cfg[0x50] >> 5)
#   the debug gate, as `1 < _dsplibs_debug_level`, which is DSPLIB_DEBUG_ON()
#   the calling convention, including __regparm2 where the object uses it
#
# and destroyed:
#
#   aggregates.  `unsigned char allow[6]` came back as `local_2c`, `local_28`
#   and `uStack_27`, written through as `local_2c._2_1_ = 1`.  An array is
#   the thing it cannot see, and arrays are most of this object.
#   types and signatures -- `(int param_1, int param_2)` for what is
#   `(void *obj, struct v34_dftbin *bins)`.  Not recoverable from the object,
#   so not a fault, but it means the data model is still yours to derive.
#
# So: it is good at the half this project finds tedious (control flow) and bad
# at the half this project's findings are actually about (struct layout,
# bitfields, fixed-point scaling).  That is a useful division of labour, and
# it is why this exists.
#
# WHAT IT IS NOT
#
# NOT EVIDENCE.  `docs/fastpass.md` says read from the disassembly and not
# from a summary of it, and this is a summary.  The rule stands:
#
#   Every line goes to `tools/dis.py` before it goes into src/.
#   No name, no comment and no finding is ever written from this output --
#   a Ghidra guess recorded as a derivation corrupts the record, and the
#   record is the deliverable.
#   The differential test remains the only thing that decides.  That is also
#   the answer to "what if the decompilation does not match the assembly":
#   it does not have to, because it is never what ships and never what is
#   trusted.  A mismatch shows up as a failing test, which is the normal
#   case this harness exists to catch.
#
# UNTESTED HERE: x87.  This object is built -mfpmath=387 and Ghidra's x87
# modelling is its known weak spot.  The float-heavy modules are already
# reconstructed, so this has not been measured -- treat any floating-point
# output as suspect until someone does.  Still true after the 12.2 trial
# below: the function picked as an x87 probe, `V34EchoFilter`, turned out to
# be fixed-point shorts.
#
# WHICH GHIDRA.  Pinned to 11.4.2, and the pin has evidence behind it rather
# than inertia: `ghidra_12.2_DEV` produces BYTE-IDENTICAL output on all eight
# functions tried, C and C++, arrays and virtual dispatch.  `allow[6]` is
# still three unrelated locals.  Two major versions, no movement on the one
# weakness that matters here, so there is nothing to gain by moving.
#
# 12.x needs setting up before it runs this at all -- Ghidra 12 hands `.py` to
# PyGhidra rather than Jython, and PyGhidra must be pip-installed first.  It
# is installed on this machine; the invocation is NOT `analyzeHeadless` but
#
#     ~/.config/ghidra/ghidra_12.2_DEV/venv/bin/python3 \
#         $G/Ghidra/Features/PyGhidra/support/pyghidra_launcher.py $G -H ...
#
# so pointing $GHIDRA at a 12.x tree makes this script fail, by design, with
# the reason printed.  Finding 703.
#
# C++ NAMES DO NOT WORK HERE.  Ghidra demangles, so ask for `resample`, never
# `_ZN9Resampler8resampleEPKfjPfRj` -- which means no C++ name as written in
# our own records will match, and the short name is ambiguous across classes.
# That wants fixing before anyone reads `VPcmV34Main.cpp` with this.  Finding
# 704.
#
# USAGE
#
#     tools/decompile.sh v34handshak
#     tools/decompile.sh probeselect chkForceBaudRate > /tmp/draft.c
#
set -eu
GHIDRA=${GHIDRA:-$HOME/ghidra/ghidra_11.4.2_PUBLIC}
HEADLESS="$GHIDRA/support/analyzeHeadless"
BLOB=${BLOB:-../slmodemd/dsplibs.o}
[ -x "$HEADLESS" ] || { echo "no analyzeHeadless at $HEADLESS; set \$GHIDRA" >&2; exit 1; }
[ $# -ge 1 ] || { sed -n '2,/^set -eu/p' "$0" >&2; exit 1; }

PROJ=$(mktemp -d)
LOG=$(mktemp)
trap 'rm -rf "$PROJ" "$LOG"' EXIT
WANT=$(echo "$@" | tr ' ' ',')
export WANT

#
# THE EXIT STATUS OF analyzeHeadless SAYS NOTHING ABOUT THE SCRIPT.
#
# It reports on the IMPORT.  A post-script that throws is logged as an ERROR
# and then `Import succeeded` is printed and 0 is returned.  So the failure
# mode is silence: no output, no diagnostic, and a shell `&&` chain that
# carries on as if the decompilation had happened.
#
# This bit, on Ghidra 12.2-DEV: `.py` is routed to PyGhidra rather than
# Jython there, PyGhidra was not installed, and the run produced zero bytes
# with status 0.  The message that explained it -- `Ghidra was not started
# with PyGhidra. Python is not available` -- went to the `2>/dev/null` this
# replaces.  Finding 703.
#
# So the log is CAPTURED rather than discarded, the extraction is checked,
# and a failure prints the ERROR lines that name the cause.  Same argument as
# gates.md: a tool nobody has seen fail is not a tool.
#
"$HEADLESS" "$PROJ" p -import "$BLOB" \
    -postScript decompile.py -scriptPath "$(dirname "$0")/ghidra" \
    -deleteProject >"$LOG" 2>&1 || true

sed -n '/^=====BEGIN/,/^=====END=====$/p' "$LOG" >"$LOG.out"
if [ ! -s "$LOG.out" ]; then
	echo "decompile.sh: $HEADLESS produced no decompilation." >&2
	echo "  ghidra: $GHIDRA" >&2
	echo "  wanted: $WANT" >&2
	grep -i 'SCRIPT ERROR\|GhidraScriptLoadException\|Python is not available' \
	    "$LOG" >&2 || echo "  (no script error in the log; ran the analysis but matched no function?)" >&2
	rm -f "$LOG.out"
	exit 1
fi
cat "$LOG.out"
rm -f "$LOG.out"
