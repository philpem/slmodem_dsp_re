#!/bin/sh
#
# run.sh -- run a command in the pinned modern-tier container.
#
# Builds tools/modern/Dockerfile if needed and runs the given command with
# this tree mounted read-write at its own path, so paths and git both work.
#
#     tools/modern/run.sh make -j4 portability
#     tools/modern/run.sh python3 tools/mutsnap.py --update --jobs 4
#
# MUTATE_WORKDIR is passed through onto the SAME host path, mounted, so the
# ~1 GB per-worker copies stay on the host's disk and reach the host's view.
# It defaults under $HOME and is created if absent.
#
# The container runs as the invoking uid:gid so anything it writes -- a
# refreshed snapshot.json -- comes back owned by you and not root.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
image=${MODERN_IMAGE:-slmodem-modern:24.04}
work=${MUTATE_WORKDIR:-$HOME/.cache/slmodem-mutate}

mkdir -p "$work"

if ! docker image inspect "$image" >/dev/null 2>&1; then
    echo "run: building $image from tools/modern/Dockerfile (GCC 13.3.0)" >&2
    docker build -t "$image" "$root/tools/modern" >&2
fi

exec docker run --rm -i \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e MUTATE_WORKDIR="$work" \
    -v "$root:$root" \
    -v "$work:$work" \
    -w "$root" \
    "$image" "$@"
