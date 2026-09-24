#!/bin/sh
#
# mutsnap-rerecord.sh -- one-shot full mutation-snapshot re-record (issue #138).
#
# Runs every registered suite with `tools/mutsnap.py --update`, refreshing the
# recorded verdicts in test/mutations/snapshot.json.  Commit the result.
#
# WHY THE COMPILER MATTERS, AND WHY --container.  A mutation verdict depends
# on the source, the test AND the compiler that built them, but the snapshot's
# key covers only the source.  The tree's modern tier is defined against GCC 13
# (`docs/method/compilers.md`, `tools/gccdiverge.json`; the historical #96
# fixture recorded Ubuntu 24.04's 13.3.0), and GCC 14 diverges (#30/#172).  So
# re-record on GCC 13, and use `--container` to get exactly that from
# `tools/modern/Dockerfile` if the host is not already 13.3.0.
#
# WHAT IT NEEDS
#   * third_party/spandsp -- gitignored, so it does NOT come with a clone.
#   * MUTATE_WORKDIR on DISK -- mutate.py refuses a tmpfs, deliberately: the
#     per-worker copies are ~1 GB each and tmpfs pages are RAM (F-2026-09-19,
#     the OOM that took a session down).  Defaults under $HOME.
#   * A few GB free under that workdir.
#
# USAGE
#   tools/mutsnap-rerecord.sh                 # host compiler
#   tools/mutsnap-rerecord.sh --container     # pinned GCC 13.3.0 container
#   JOBS=8 tools/mutsnap-rerecord.sh          # default: --jobs is half the cores
#
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

container=0
for arg in "$@"; do
    case "$arg" in
        --container) container=1 ;;
        *) echo "mutsnap-rerecord: unknown argument: $arg" >&2; exit 2 ;;
    esac
done

# --- prerequisites ---------------------------------------------------------
# spandsp is gitignored, and some suites link it, so it must be present AND
# BUILT (the static library, not just the source tree).
spandsp_lib=third_party/spandsp/src/.libs/libspandsp.a
if [ ! -f "$spandsp_lib" ]; then
    echo "mutsnap-rerecord: $spandsp_lib is missing." >&2
    echo "  third_party/spandsp is gitignored, so a clone does not bring it." >&2
    echo "  Build the pinned peer per third_party/README.md:" >&2
    echo "    git clone --depth 1 --branch version-3.1.0 \\" >&2
    echo "        https://github.com/freeswitch/spandsp third_party/spandsp" >&2
    echo "    cd third_party/spandsp && ./bootstrap.sh && \\" >&2
    echo "        ./configure --disable-shared --enable-static && make" >&2
    exit 1
fi

# mutate.py enforces the disk contract too, but say it early and by name.
work=${MUTATE_WORKDIR:-$HOME/.cache/slmodem-mutate}
mkdir -p "$work"
export MUTATE_WORKDIR=$work

jobs_opt=""
[ -n "${JOBS:-}" ] && jobs_opt="--jobs $JOBS"

# --- run -------------------------------------------------------------------
if [ "$container" = 1 ]; then
    echo "mutsnap-rerecord: container (GCC 13.3.0), workdir $MUTATE_WORKDIR"
    # shellcheck disable=SC2086  # jobs_opt is deliberately split
    exec "$root/tools/modern/run.sh" python3 tools/mutsnap.py --update $jobs_opt
fi

echo "mutsnap-rerecord: host toolchain (must be GCC 13 for a comparable record):"
gcc --version | head -1
ld --version | head -1
echo "mutsnap-rerecord: workdir $MUTATE_WORKDIR"
echo "mutsnap-rerecord: this records ~10,038 mutations; expect hours."
# shellcheck disable=SC2086  # jobs_opt is deliberately split
exec python3 tools/mutsnap.py --update $jobs_opt
