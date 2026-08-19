#!/bin/bash
#
# hybrid_link.sh -- link a V.34 hybrid slmodemd after tools/hybrid.sh.
#
# Usage: tools/hybrid_link.sh OUTDIR OUR_OBJECTS...
#
# The datapump objects must be supplied on the link line: hybrid.sh weakens
# their blob counterparts but deliberately does not replace them.
set -eu

OUT=${1:?usage: hybrid_link.sh OUTDIR OUR_OBJECTS...}
shift
# The active D-Modem integration tree.  A worktree or another checkout may
# supply FORK explicitly.
FORK=${FORK:-/home/philpem/dev/sip-D-modem/d-modem/slmodemd}

[ $# -gt 0 ] || { echo "hybrid_link: no object files given" >&2; exit 2; }

stale=0
for o in "$@"; do
	# Both build/src/... and an isolated build/NAME/src/... are valid.  The
	# latter is used for compile-time-gated diagnostic artefacts.
	src=$(echo "$o" | sed 's|.*/build/src/|src/|; s|.*/build/[^/]*/src/|src/|; s|\.o$|.c|')
	[ -f "$src" ] || src=${src%.c}.cpp
	[ -f "$src" ] || continue
	if [ "$src" -nt "$o" ]; then
		echo "hybrid_link: STALE $o is older than $src" >&2
		stale=1
	fi
done
[ "$stale" -eq 0 ] || {
	echo "hybrid_link: run make first -- refusing to link stale objects" >&2
	exit 2
}

# The D-Modem bridge owns VPCMXF_Create.  Keep the blob copy under a private
# name for its internal caller rather than linking our conflicting definition.
OURS=""
for o in "$@"; do
	case "$o" in *VPcmXfCreate.o) continue ;; esac
	OURS="$OURS $o"
done

[ -f "$OUT/dsplibs_hybrid.o" ] || {
	echo "hybrid_link: $OUT/dsplibs_hybrid.o missing -- run hybrid.sh first" >&2
	exit 2
}

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
cc -m32 -o "$OUT/slmodemd-fit" \
	$OBJS $OURS "$OUT/dsplibs_hybrid_r.o" \
	-Wl,-Map,"$OUT/link.map" -lm

echo "hybrid_link: wrote $OUT/slmodemd-fit"
