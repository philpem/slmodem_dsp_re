#!/bin/sh
# How many checks each differential binary actually ran.
#
# WHY THIS IS A SEPARATE PASS AND NOT A `make phase` LOG PARSE.  The harness
# prints `PASS <section> <n> checks` per section and never names its own
# binary.  Under `-jN` three binaries' sections interleave in the log and no
# line says whose is whose, so the counts cannot be attributed after the fact.
# Running each binary alone is the only way to know, and it is cheap: the
# whole suite is a couple of minutes.
#
# Writes `<binary> <total checks>` to build/eqproof_checks.txt, which
# `tools/eqproof.py` reads.  A binary that fails still gets a line, with
# whatever it managed -- this is a measurement of the suite, not a gate, and a
# failing suite is `make phase`'s business.
#
# Usage:  sh tools/eqproof-checks.sh  [BUILD]

set -e
root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-$root/build}
out=$build/eqproof_checks.txt

if [ ! -d "$build/test" ]; then
    echo "eqproof-checks.sh: no $build/test -- run \`make phase\` first." >&2
    exit 1
fi

n=0
: > "$out"
for b in "$build"/test/t_*; do
    [ -x "$b" ] || continue
    [ -f "$b" ] || continue
    name=$(basename "$b")
    #
    # BOTH VERDICT LINES, AND THAT IS NOT TIDINESS.  The harness prints
    # `PASS <section> <n> checks` and `FAIL <section> <a>/<b> checks failed`,
    # and counting only the first reads a FAILING section as ZERO CHECKS.
    # Eight binaries carry a check `tools/gccdiverge.json` declares -- a site
    # where modern GCC provably cannot reproduce the object -- so they exit
    # non-zero here while `make period` passes them and `make phase` is green.
    # `t_v90specproc` runs 35,450 checks and was being reported as 0, which put
    # `V90SpectralVerifier::process` in eqproof's AD-HOC VECTORS class: the
    # weakest class in the report, occupied by one symbol, for a reason that
    # was this script's.
    #
    total=$("$b" 2>/dev/null |
            sed -n -e 's/^PASS .* \([0-9][0-9]*\) checks$/\1/p' \
                   -e 's/^FAIL .* [0-9][0-9]*\/\([0-9][0-9]*\) checks failed$/\1/p' |
            awk '{s+=$1} END {print s+0}')
    printf '%s %s\n' "$name" "$total" >> "$out"
    n=$((n + 1))
done

if [ "$n" -eq 0 ]; then
    echo "eqproof-checks.sh: ZERO binaries run.  That is a refusal and not a" >&2
    echo "  clean sheet -- findings F134, F2400." >&2
    rm -f "$out"
    exit 1
fi
echo "eqproof-checks.sh: $n binaries, totals in $out"
