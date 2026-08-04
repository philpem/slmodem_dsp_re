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
# output as suspect until someone does.
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
trap 'rm -rf "$PROJ"' EXIT
WANT=$(echo "$@" | tr ' ' ',')
export WANT
# Ghidra is chatty on stderr and the analysis log is not the deliverable.
"$HEADLESS" "$PROJ" p -import "$BLOB" \
    -postScript decompile.py -scriptPath "$(dirname "$0")/ghidra" \
    -deleteProject 2>/dev/null |
  sed -n '/^=====BEGIN/,/^=====END=====$/p'
