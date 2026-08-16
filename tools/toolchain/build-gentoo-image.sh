#!/bin/sh
#
# Build `dsplibs-tc342-gentoo` -- Gentoo's gcc-3.4.2-r2, the compiler named in
# the blob's .comment, built from Gentoo's ebuild inside Gentoo's 2005.0
# userland.  See Dockerfile.gentoo for what that image is and is not.
#
# Two things have to be on this machine and neither is in git:
#
#   the stage3      stage3-x86-2005.0.tar.bz2, 88,982,705 bytes
#                   md5 4ea12f4ae446c72164c28aa0b836453e
#   the distfiles   the six SRC_URI files; gentoo-3.4.2-r2/fetch-distfiles.sh
#                   fetches and verifies them against Gentoo's own digest
#
# Override either with STAGE3=/path/to/tarball or DISTFILES=/path/to/dir.
#
set -e
cd "$(dirname "$0")"

REC=${REC:-$HOME/Downloads/gentoo-gcc-3.4.2-r2-recovery}
STAGE3=${STAGE3:-$REC/stage3/stage3-x86-2005.0.tar.bz2}
DISTFILES=${DISTFILES:-$REC/distfiles}
BASE=${BASE:-gentoo-2005.0-stage3:x86}
IMAGE=${IMAGE:-dsplibs-tc342-gentoo}

for f in gcc-3.4.2.tar.bz2 gcc-3.4.2-patches-1.1.tar.bz2 \
         gcc-3.4.0-piepatches-v8.7.6.5.tar.bz2 protector-3.4.1-1.tar.gz; do
    [ -f "$DISTFILES/$f" ] || {
        echo "missing $DISTFILES/$f -- run gentoo-3.4.2-r2/fetch-distfiles.sh" >&2
        exit 1
    }
done

# THE BASE IS AN IMPORT, NOT A PULL.  No registry carries a 2005 Gentoo, and
# the point of this image is that the userland is period too: glibc 2.3.4 and
# binutils 2.15.92.0.2-r1 are what the object's builder had.  It runs unchanged
# on a 2026 kernel -- that was the risk, and it is not one.
if ! docker image inspect "$BASE" > /dev/null 2>&1; then
    [ -f "$STAGE3" ] || { echo "no stage3 at $STAGE3" >&2; exit 1; }
    echo "$STAGE3: verifying"
    echo "4ea12f4ae446c72164c28aa0b836453e  $STAGE3" | md5sum -c - \
        || { echo "stage3 md5 mismatch -- refusing to import" >&2; exit 1; }
    echo "importing $BASE"
    docker import --platform linux/386 "$STAGE3" "$BASE"
fi

# `--build-context` rather than copying 28 MB of tarballs into the repo or into
# a staging directory.  Requires buildkit, which is the default.
exec docker build --platform linux/386 \
    -f Dockerfile.gentoo \
    --build-context "distfiles=$DISTFILES" \
    -t "$IMAGE" .
