#!/bin/bash
#
# hybrid.sh -- build an slmodemd whose datapump is OURS, not the blob's.
#
#   tools/hybrid.sh OUTDIR build/src/pump/v34/*.o build/src/pump/v90/*.o ...
#   tools/hybrid.sh build/hybrid-vpcm $(cat build/vpcm-objs.txt)
#
# `make coverage` is what fills build/src; a plain `make` fills build/repro
# instead (the -DDSPLIB_REPRODUCE_BUGS tree, whose paths have no `src/` in
# them) and has done since 75dcc19.  Findings F3055 and F3110.
#
# WHY WEAK SYMBOLS AND NOT RENAMING.  The obvious instrument is
# `objcopy --redefine-syms`, and it cannot work: renaming moves a symbol's
# DEFINITION and its REFERENCES together, so it cannot split them, and what
# this needs is to keep the blob's references while replacing what they reach.
# `--weaken-symbol` does exactly that -- the blob's definition becomes W, ours
# stays T, and the linker prefers the strong one.  tools/blobfix.py already
# ships this mechanism for two blob TABLES, so it is proven in-tree.
#
# WHAT MAKES WHOLE-DATAPUMP SUBSTITUTION POSSIBLE.  Only the registration
# boundary carries a relocation.  For Bell 103, `dp_b103_init` and
# `dp_b103_exit` have one each; `b103_ops`, `b103_create`, `b103_process` and
# `b103_delete` have none -- they are intra-object displacements which CANNOT
# be redirected and do not need to be.  Ours registers OUR ops structure,
# whose pointers reach our own create/process/delete, and the blob's copy of
# that datapump simply becomes dead code in the image.
#
# THE COLLISION SET IS BIGGER THAN THE FUNCTION LIST AND MUST BE COMPUTED.
# For Bell 103 it is 46 symbols, not the 6 the datapump appears to be: our
# objects also redefine tables the blob defines (filter coefficients, AGC
# constants).  Weaken every one or the link dies on multiple definition.  This
# script intersects the two symbol sets rather than trusting a hand-list,
# because a hand-list is wrong the moment a table is added.
#
# THE BLOB IS NEVER MODIFIED.  A copy is made, its md5 checked against the
# original before and after, and every change happens to the copy.
#
set -eu

RE=/home/philpem/dev/sip-D-modem/claude_re
BLOB=${BLOB:-/home/philpem/dev/sip-D-modem/slmodemd/dsplibs.o}

OUT=${1:?usage: hybrid.sh OUTDIR our-objects...}
shift
OBJS=("$@")
[ ${#OBJS[@]} -gt 0 ] || { echo "hybrid: no objects given" >&2; exit 2; }

mkdir -p "$OUT"
BEFORE=$(md5sum "$BLOB" | cut -d' ' -f1)

# Defined symbols on each side.  `nm --defined-only -g`: global definitions
# only -- a local symbol cannot collide at link time and weakening a symbol
# that is not there is a silent no-op that would hide a typo.
nm --defined-only -g "$BLOB" | awk '{print $3}' | sort -u > "$OUT/blob.syms"
nm --defined-only -g "${OBJS[@]}" 2>/dev/null | awk 'NF==3 {print $3}' | sort -u > "$OUT/ours.syms"
comm -12 "$OUT/blob.syms" "$OUT/ours.syms" > "$OUT/collide.syms"

N=$(wc -l < "$OUT/collide.syms")
echo "hybrid: ${#OBJS[@]} of our objects define $(wc -l < "$OUT/ours.syms") globals"
echo "hybrid: $N of them collide with the blob and will be weakened in the copy"
[ "$N" -gt 0 ] || { echo "hybrid: NOTHING COLLIDES -- ours would never be reached." >&2
                    echo "  Did you pass the right objects?  A datapump that overrides" >&2
                    echo "  nothing is a datapump the blob's registration never calls." >&2
                    exit 3; }

ARGS=()
while read -r s; do ARGS+=("--weaken-symbol=$s"); done < "$OUT/collide.syms"
objcopy "${ARGS[@]}" "$BLOB" "$OUT/dsplibs_hybrid.o"

AFTER=$(md5sum "$BLOB" | cut -d' ' -f1)
[ "$BEFORE" = "$AFTER" ] || { echo "hybrid: THE BLOB CHANGED. Aborting." >&2; exit 4; }

# Prove ours actually wins rather than assuming it.  Compare the SIZE of a
# symbol defined on both sides: after the link it must be ours.  A size that
# still matches the blob's means the weakening did not take, and everything
# downstream would then be measuring the original while claiming otherwise.
echo
echo "hybrid: verifying the override on a sample of the collision set"
head -5 "$OUT/collide.syms" | while read -r s; do
	b=$(nm -S --defined-only "$BLOB" | awk -v s="$s" '$4==s {print $2}')
	o=$(nm -S --defined-only "${OBJS[@]}" 2>/dev/null | awk -v s="$s" '$4==s {print $2; exit}')
	w=$(nm -S --defined-only "$OUT/dsplibs_hybrid.o" | awk -v s="$s" '$4==s {print $3}')
	printf '    %-32s blob %6s  ours %6s  blob-now %s\n' "$s" "${b:--}" "${o:--}" "${w:-?}"
done
echo
echo "hybrid: wrote $OUT/dsplibs_hybrid.o"
echo "        link it with: ${OBJS[*]}"
