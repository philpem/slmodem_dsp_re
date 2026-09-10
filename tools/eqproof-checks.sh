#!/bin/sh
# Run binaries separately so parallel harness sections cannot be misattributed.
# Python preserves return codes, both output streams, and PASS/FAIL denominators.
# Usage: sh tools/eqproof-checks.sh [BUILD] [--layout modern|period] [--timeout N]
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec python3 "$root/tools/eqproof.py" --collect "$@"
