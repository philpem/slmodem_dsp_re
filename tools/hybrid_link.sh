#!/bin/bash
#
# hybrid_link.sh -- link the hybrid slmodemd that `hybrid.sh` stops short of.
#
#   tools/hybrid_link.sh build/hybrid-fit
#
# `hybrid.sh` produces `dsplibs_hybrid.o` -- the blob with our datapump's
# symbols weakened out of it -- and then prints "link it with: ..." and stops.
# The remaining two steps were done by hand, and the recipe existed only in one
# session's scrollback: the binary on the bench was an HOUR OLDER than the
# source it was supposed to be testing, and nothing said so.  That is what this
# script exists to prevent.
#
# STEP 1, THE `objcopy`.  `VPCMXF_Create` must be renamed rather than weakened.
# Weakening splits a definition from its references, which is what we want
# everywhere else; here the blob's own copy has to stay reachable under a
# different name because our `VPcmXfCreate.o` defines the strong one and the
# blob still needs its original at a different call site.  Renaming moves
# definition AND references together, which is exactly right for this one
# symbol and exactly wrong for the other 618.
#
# STEP 2, THE LINK.  32-bit, against D-Modem-fork's slmodemd objects rather
# than this repo's -- this tree's slmodemd has no `-e` option, and linking the
# wrong one cost four bench calls before anyone noticed the binary could not
# take the front end it was being handed.
#
# `benchflags.o` goes in LAST so its constructor is registered after the
# datapump's globals exist; it reads the DSPLIB_V34_* environment and is the
# only way the gated experiments are switched on.
#
set -eu

OUT=${1:?usage: hybrid_link.sh OUTDIR our-objects...}
shift
FORK=${FORK:-/home/philpem/dev/D-Modem-fork/slmodemd}

# OUR OBJECTS GO ON THE LINK LINE TOO, and this is the trap.  `hybrid.sh`
# weakens the blob's copies of the 618 colliding symbols; it does not supply
# replacements.  Those live in the .o files it printed after "link it with:",
# and without them the weakened definitions are all that remain -- or, for a
# symbol we define that the blob never had, nothing remains at all.  The first
# attempt at this script omitted them and failed on exactly that: the four
# `dsplib_v34_*` experiment flags are ours alone, so they were undefined
# references rather than a silent fallback to the blob's behaviour.  The
# undefined reference is lucky; a weakened-but-present symbol would have linked
# and quietly run the BLOB's code while claiming to test ours.
[ $# -gt 0 ] || { echo "hybrid_link: no object files given" >&2; exit 2; }

# VPcmXfCreate.o IS DROPPED, deliberately.  The fork's dp_vpcm_shim.o DEFINES
# `VPCMXF_Create` (it is slmodemd's entry point) and REFERENCES
# `__blob_VPCMXF_Create`, which the objcopy below supplies from the blob.  Our
# VPcmXfCreate.o defines the same name, so linking it collides with the shim.
# The shim has to win -- it is what slmodemd calls -- so ours comes out.
#
# HARMLESS FOR WHAT THIS BINARY IS FOR: VPCMXF_Create is the V.90/V.92 PCM
# construction path, and this build exists to bench the V.34 datapump.  It does
# mean a VPCM measurement taken with this binary is the BLOB's, not ours; say
# so if one is ever taken, or build a separate hybrid that resolves it the
# other way.
OURS=""
for o in "$@"; do
	case "$o" in *VPcmXfCreate.o) continue ;; esac
	OURS="$OURS $o"
done

[ -f "$OUT/dsplibs_hybrid.o" ] || {
	echo "hybrid_link: $OUT/dsplibs_hybrid.o missing -- run hybrid.sh first" >&2
	exit 2
}

# The slmodemd half.  dp_dummy/dp_sinus are the built-in test datapumps, and
# dp_v8_shim/dp_vpcm_shim/v8_open_stub are the fork's bridge into the blob's
# V.8 and PCM entry points.  modem_test.o is deliberately absent: it carries a
# second `main`.
SL="dp_dummy dp_sinus dp_v8_shim dp_vpcm_shim homolog_data modem_at
    modem_cmdline modem_comp modem_datafile modem_debug modem_ec modem_main
    modem modem_pack modem_param modem_timer sysdep_common v8_open_stub"

OBJS=""
for o in $SL; do
	[ -f "$FORK/$o.o" ] || { echo "hybrid_link: missing $FORK/$o.o" >&2; exit 2; }
	OBJS="$OBJS $FORK/$o.o"
done

objcopy --redefine-sym VPCMXF_Create=__blob_VPCMXF_Create \
	"$OUT/dsplibs_hybrid.o" "$OUT/dsplibs_hybrid_r.o"

[ -f "$OUT/benchflags.o" ] || \
	cc -m32 -c -O2 -o "$OUT/benchflags.o" "$(dirname "$0")/benchflags.c"

cc -m32 -o "$OUT/slmodemd-fit" \
	$OBJS $OURS "$OUT/dsplibs_hybrid_r.o" "$OUT/benchflags.o" \
	-Wl,-Map,"$OUT/link.map" -lm

echo "hybrid_link: wrote $OUT/slmodemd-fit"
"$OUT/slmodemd-fit" --help >/dev/null 2>&1 ||
	"$OUT/slmodemd-fit" -h >/dev/null 2>&1 || true
echo "hybrid_link: gated flags it will honour:"
strings "$OUT/slmodemd-fit" | grep -oE 'DSPLIB_V34_[A-Z_]+' | sort -u | sed 's/^/    /'
