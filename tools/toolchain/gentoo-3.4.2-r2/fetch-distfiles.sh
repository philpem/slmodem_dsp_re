#!/bin/sh
#
# Fetch the six source archives gcc-3.4.2-r2 was built from, and verify them.
#
# WHY THIS SCRIPT EXISTS AND THE TARBALLS DO NOT.  The archives are 28 MB and
# git is the wrong place for them; the RECIPE beside this script is text and
# belongs in the tree.  This closes the gap: it turns the recorded URLs into
# something that runs, so the recovery is reproducible rather than merely
# described.  If the tarballs are ever brought into the repository proper it
# will be through LFS, and this script stays useful either way.
#
# WHAT IS AT STAKE.  The object under reconstruction names its compiler 279
# times over:
#
#     GCC: (GNU) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)
#
# That is Gentoo's build, not stock GNU 3.4.2, and the difference is these
# patch sets.  Note the DOUBLE SPACE after "3.4.2": `toolchain.eclass` calls
# `gcc_version_patch "${BRANCH_UPDATE} (${release_version})"` with
# BRANCH_UPDATE empty, so the argument carries a leading space and the sed at
# line 607 adds another.  That anomaly is a fingerprint of this exact
# ebuild/eclass pair, and it is why we believe the recipe beside this script
# is the right one rather than merely a plausible one.
#
# TWO OF THESE FILES EXIST IN EXACTLY ONE PLACE ON THE PUBLIC INTERNET.
# `gcc-3.4.2-patches-1.1.tar.bz2` and `gcc-3.4.0-piepatches-v8.7.6.5.tar.bz2`
# are on one person's home directory and nowhere else -- 852 probes across 284
# Gentoo mirrors, using the correct BLAKE2B-sharded paths, found neither.  If
# you have a copy, keep it: they are the two files that make this the Gentoo
# compiler instead of a stock one.
#
# A NOTE ON PROBING MIRRORS, since it invalidated a whole sweep once.  Gentoo
# shards distfiles as `distfiles/<2-hex>/<filename>`, where the hex is the
# first 8 bits of BLAKE2B *of the filename*.  A flat-path probe 404s whether
# the file is there or not.  And several hosts return 200 with HTML for any
# name, while FTP mirrors answer 221 to nonsense.  A 200 needs a negative
# control and a 404 needs a positive one.
#
set -e
#
# RESOLVED BEFORE THE cd, because `dirname "$0"` is relative to the caller's
# working directory and this script changes it.  Getting that wrong made the
# md5 step look for the digest inside the download directory and report
# VERIFICATION FAILED on a set of files that were in fact perfect.
#
HERE=$(cd "$(dirname "$0")" && pwd)
DEST=${1:-distfiles}
mkdir -p "$DEST"
cd "$DEST"

# name  primary-url  [alternate-url ...]
# Primary is the most durable source known, not necessarily where our copy
# came from: gcc-3.4.2.tar.bz2 is still on canonical upstream and should be
# taken from there.
set -- \
"gcc-3.4.2.tar.bz2|https://gcc.gnu.org/pub/gcc/releases/gcc-3.4.2/gcc-3.4.2.tar.bz2|http://bloodnoc.org/~roy/olde-distfiles/gcc-3.4.2.tar.bz2" \
"protector-3.4.1-1.tar.gz|https://grok.org.uk/tools/ssp/protector-3.4.1-1.tar.gz|http://bloodnoc.org/~roy/olde-distfiles/protector-3.4.1-1.tar.gz|https://www.jabawok.net/gentoo/distfiles/protector-3.4.1-1.tar.gz" \
"gcc-3.4.2-patches-1.1.tar.bz2|http://bloodnoc.org/~roy/olde-distfiles/gcc-3.4.2-patches-1.1.tar.bz2" \
"gcc-3.4.0-piepatches-v8.7.6.5.tar.bz2|http://bloodnoc.org/~roy/olde-distfiles/gcc-3.4.0-piepatches-v8.7.6.5.tar.bz2" \
"gcc-3.4.2-manpages.tar.bz2|http://bloodnoc.org/~roy/olde-distfiles/gcc-3.4.2-manpages.tar.bz2" \
"bounds-checking-gcc-3.4.2-1.00.patch.bz2|http://bloodnoc.org/~roy/olde-distfiles/bounds-checking-gcc-3.4.2-1.00.patch.bz2"

for spec in "$@"; do
	name=$(echo "$spec" | cut -d'|' -f1)
	urls=$(echo "$spec" | cut -d'|' -f2- | tr '|' ' ')
	if [ -f "$name" ]; then
		echo "  have    $name"
		continue
	fi
	got=""
	for u in $urls; do
		echo "  fetch   $name  <- $u"
		if curl -fsSL --max-time 600 -o "$name.part" "$u"; then
			mv "$name.part" "$name"; got=1; break
		fi
		rm -f "$name.part"
		echo "          failed, trying next source"
	done
	[ -n "$got" ] || echo "  MISSING $name -- every source failed"
done

#
# THE CHECKSUMS ARE GENTOO'S OWN, from the digest file in the recipe beside
# this script, not computed from what we happened to download.  A file that
# arrives with the wrong md5 is not the file, however plausible its name.
#
echo
if md5sum -c "$HERE/MD5SUMS"; then
	echo
	echo "All six verified against Gentoo's digest."
	echo "The two irreplaceable ones are gcc-3.4.2-patches-1.1.tar.bz2 and"
	echo "gcc-3.4.0-piepatches-v8.7.6.5.tar.bz2 -- back them up somewhere else."
else
	echo
	echo "VERIFICATION FAILED.  Do not build from these."
	exit 1
fi
