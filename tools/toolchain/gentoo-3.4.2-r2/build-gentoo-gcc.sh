#!/bin/bash
#
# gcc-3.4.2-r2, THE WAY THE EBUILD BUILDS IT, by hand.
#
# This runs INSIDE the Gentoo 2005.0 x86 stage3 (glibc 2.3.4, binutils
# 2.15.92.0.2-r1, gcc 3.3.5 as the bootstrap compiler).  The stage3 has no
# portage tree -- 0 entries -- so `emerge` is not available and this script
# reproduces what `sys-devel/gcc/gcc-3.4.2-r2.ebuild` and `toolchain.eclass`
# do, in their order, from the same six SRC_URI files.
#
# The ebuild and eclass in ../build-recipe/ are the specification.  Every step
# below cites the line it comes from, and anything this script does NOT do is
# listed under DELIBERATE DEVIATIONS at the bottom.
#
# The acceptance test is one string:
#
#     gcc (GCC) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)
#
# with TWO spaces after 3.4.2 -- gcc_version_patch (eclass:604) is called with
# an empty BRANCH_UPDATE, so its argument carries a leading space, and the sed
# at eclass:607 emits `\1 @GENTOO@\2`, contributing another.  Finding F2320.
#
# EVERY PATCH IS FATAL IF IT DOES NOT APPLY, and the count is printed.  That
# matters more than it looks: gcc_version_patch is the LAST step and is only
# two seds on gcc/version.c, so a tree with half the patch stack silently
# skipped still prints the exact version string above.  The string identifies
# the recipe; only the patch count says the recipe ran.
#
set -e
export LC_ALL=C		# glob and sort order: Gentoo's own, where 100_ sorts
			# between 09_ and 10_.  Do not "fix" this.

WORKDIR=${WORKDIR:-/build}
DISTDIR=${DISTDIR:-/distfiles}
FILESDIR=${FILESDIR:-/recipe/build-recipe/in-tree-files}
JOBS=${JOBS:-6}

# From the ebuild's variables and the stage3's own /etc/make.conf.
PV=3.4.2
PVR=3.4.2-r2
MY_PV=3.4
MY_PV_FULL=3.4.2
PP_FVER=3.4.1-1		# ebuild:52-53, PP_VER="3_4_1"
PIE_VER=8.7.6.5		# ebuild:50
CHOST=${CHOST:-i386-pc-linux-gnu}
CCHOST=$CHOST

S=$WORKDIR/gcc-$PV

# ---------------------------------------------------------------- epatch ---
#
# eutils' epatch, reduced to what this build needs and made fail-loud.  Two
# behaviours are reproduced because the recipe depends on them:
#
#   - a DIRECTORY argument applies every `*.patch.bz2` in it, in glob order
#     (EPATCH_SUFFIX defaults to patch.bz2 in the eutils of the period);
#   - the patch level is not written down anywhere, so 0..4 are tried and the
#     first that applies cleanly in a dry run is used.
#
# Arch filtering does not arise: every file in patch/ and piepatch/ is named
# `_all_`, which was checked rather than assumed.
#
NPATCH=0
apply_one() {
	local f=$1 lvl
	[ -f "$f" ] || { echo "!! no such patch: $f" >&2; exit 1; }
	case "$f" in
	*.bz2)	bzip2 -dc "$f" > /tmp/epatch.diff ;;
	*)	cp "$f" /tmp/epatch.diff ;;
	esac
	for lvl in 0 1 2 3 4; do
		if patch -p$lvl -f --dry-run -s < /tmp/epatch.diff > /dev/null 2>&1; then
			patch -p$lvl -f -s < /tmp/epatch.diff \
				|| { echo "!! applied in dry run, failed for real: $f" >&2; exit 1; }
			NPATCH=$((NPATCH + 1))
			printf '  [%2d] -p%d  %s\n' "$NPATCH" "$lvl" "${f##*/}"
			return 0
		fi
	done
	echo "!! NO PATCH LEVEL APPLIES: $f" >&2
	patch -p1 -f --dry-run < /tmp/epatch.diff 2>&1 | head -n 20 >&2
	exit 1
}

epatch_dir() {
	local d=$1 f n=0
	[ -d "$d" ] || { echo "!! no such patch directory: $d" >&2; exit 1; }
	for f in "$d"/*.patch.bz2; do
		[ -e "$f" ] || { echo "!! no *.patch.bz2 in $d" >&2; exit 1; }
		apply_one "$f"
		n=$((n + 1))
	done
	echo "  -- $n patches from ${d##*/}/"
}

# ---------------------------------------------------------------- unpack ---
#
# gcc_quick_unpack (eclass:293) then gcc_src_unpack (eclass:676).
#
do_unpack() {
	mkdir -p "$WORKDIR"
	cd "$WORKDIR"

	echo "== unpacking (eclass:293 gcc_quick_unpack)"
	tar xjf "$DISTDIR/gcc-$PV.tar.bz2"
	tar xjf "$DISTDIR/gcc-$PV-patches-1.1.tar.bz2"		# -> patch/
	# "The gcc 3.4 propolice versions are meant to be unpacked to ${S}"
	( cd "$S" && tar xzf "$DISTDIR/protector-$PP_FVER.tar.gz" )
	tar xjf "$DISTDIR/gcc-3.4.0-piepatches-v$PIE_VER.tar.bz2"	# -> piepatch/

	# exclude_gcc_patches (eclass:349): GENTOO_PATCH_EXCLUDE and
	# PIEPATCH_EXCLUDE are both unset for this ebuild -- the one candidate is
	# commented out at ebuild:60 -- so nothing is excluded.

	local release_version="Gentoo Linux $PVR"		# eclass:677
	cd "$S"

	echo "== the Gentoo patch set (eclass:685)"
	epatch_dir "$WORKDIR/patch"

	echo "== ProPolice (eclass:375 do_gcc_SSP_patches)"
	# GCCMINOR=4 and PP_VER=3_4_1, so the third branch runs: the patch is
	# named for the version and ships its own docs.  pro-police-docs.patch is
	# therefore NOT applied -- it is guarded on sspdocs="no" (eclass:405), and
	# RECOVERY.txt listing it as applied is wrong.  The tarball shipping
	# gcc_3_4_1.dif rather than protector.dif confirms which branch this is.
	apply_one "$S/gcc_3_4_1.dif"
	sed -e 's|^CRTSTUFF_CFLAGS = |CRTSTUFF_CFLAGS = -fno-stack-protector-all |' \
		-i gcc/Makefile.in
	# update_gcc_for_libc_ssp (eclass:427): libc_has_ssp tests the LIVE libc
	# for __guard and __stack_smash_handler.  Gentoo's glibc 2.3.4 in this
	# stage3 has both -- tested, not assumed -- so the define goes in.  It
	# reaches libgcc only, never the compiler's output.
	if readelf -s /lib/libc.so.6 | grep GLOBAL | grep -q '__guard' &&
	   readelf -s /lib/libc.so.6 | grep GLOBAL | grep -q '__stack_smash_handler'; then
		echo "  libc has ssp: adding -D_LIBC_PROVIDES_SSP_ to LIBGCC2_CFLAGS"
		sed -e 's|^\(LIBGCC2_CFLAGS.*\)$|\1 -D_LIBC_PROVIDES_SSP_|' \
			-i gcc/Makefile.in
	else
		echo "!! libc has no ssp symbols -- not what this stage3 should be" >&2
		exit 1
	fi
	release_version="$release_version, ssp-$PP_FVER"	# eclass:420

	echo "== PIE (eclass:437 do_gcc_PIE_patches)"
	epatch_dir "$WORKDIR/piepatch/upstream"
	epatch_dir "$WORKDIR/piepatch/nondef"
	epatch_dir "$WORKDIR/piepatch/def"
	release_version="$release_version, pie-$PIE_VER"	# eclass:447

	# gcc-compiler-src_unpack (eclass:661) does nothing without USE=hardened,
	# and make_gcc_hard is what would have made PIE+SSP the DEFAULT.  It is not
	# run, which is why the object has no __guard and no get_pc_thunk (606).

	echo "== version string (eclass:604 gcc_version_patch)"
	# BRANCH_UPDATE is empty, so this argument begins with a space (eclass:709).
	gcc_version_patch " ($release_version)"

	# eclass:712 -- Redhat misdesign in libstdc++
	cp -a "$S/libstdc++-v3/config/cpu/i486/atomicity.h" \
	      "$S/libstdc++-v3/config/cpu/i386/atomicity.h"
	# eclass:719 -- keep --as-needed out of the specs
	sed -i -e s/HAVE_LD_AS_NEEDED/USE_LD_AS_NEEDED/g "$S/gcc/config.in"
	# eclass:727 -- touch generated files so nothing is regenerated.  Not
	# cosmetic here: the stage3 has no gperf, so a rebuild of c-gperf.h fails.
	./contrib/gcc_update --touch > /dev/null 2>&1

	echo "== FILESDIR patches (ebuild:153-172)"
	# These come AFTER gcc_version_patch: the ebuild's src_unpack calls
	# gcc_src_unpack first and patches afterwards.  RECOVERY.txt has the two
	# the other way round.  mips and amd64-multilib patches do not apply here.
	apply_one "$FILESDIR/3.4.0/gcc34-reiser4-fix.patch"
	apply_one "$FILESDIR/gcc-spec-env.patch"
	apply_one "$FILESDIR/3.4.2/gcc34-m32-no-sse2.patch"
	apply_one "$FILESDIR/3.4.2/gcc34-fix-sse2_pinsrw.patch"

	echo "== $NPATCH patches applied, 0 skipped"
	echo -n "== version_string now: "
	grep -n 'version_string' "$S/gcc/version.c" | head -n 2
}

gcc_version_patch() {
	[ -z "$1" ] && { echo "!! no arguments to gcc_version_patch" >&2; exit 1; }
	sed -i -e 's~\(const char version_string\[\] = ".....\).*\(".*\)~\1 @GENTOO@\2~' "$S/gcc/version.c"
	sed -i -e "s:@GENTOO@:$1:g" "$S/gcc/version.c"
	sed -i -e 's~http:\/\/gcc\.gnu\.org\/bugs\.html~http:\/\/bugs\.gentoo\.org\/~' "$S/gcc/version.c"
}

# --------------------------------------------------------------- compile ---
#
# gcc_do_configure (eclass:841) + gcc-compiler-configure (eclass:783), with the
# USE flags of a vanilla (non-hardened, non-uclibc, non-gcj) x86 profile.
#
# The paths are Gentoo's real versioned ones, which is why installing over the
# stage3's own gcc 3.3.5 is safe: 3.3.5 lives in /usr/lib/gcc-lib/CHOST/3.3.5
# and this goes in /usr/lib/gcc/CHOST/3.4.2.  gcc-config existed to switch
# between exactly these; here PATH does it.
#
LIBPATH=/usr/lib/gcc/$CCHOST/$MY_PV_FULL
BINPATH=/usr/$CCHOST/gcc-bin/$MY_PV
DATAPATH=/usr/share/gcc-data/$CCHOST/$MY_PV
STDCXX_INCDIR=$LIBPATH/include/g++-v3

do_compile() {
	mkdir -p "$WORKDIR/build"
	cd "$WORKDIR/build"

	echo "== configure"
	"$S"/configure \
		--enable-version-specific-runtime-libs \
		--prefix=/usr \
		--bindir="$BINPATH" \
		--includedir="$LIBPATH/include" \
		--datadir="$DATAPATH" \
		--mandir="$DATAPATH/man" \
		--infodir="$DATAPATH/info" \
		--with-gxx-include-dir="$STDCXX_INCDIR" \
		--build=$CHOST --host=$CHOST --target=$CCHOST \
		--disable-nls \
		--enable-__cxa_atexit \
		--enable-clocale=gnu \
		--enable-shared \
		--with-system-zlib \
		--disable-checking \
		--disable-werror \
		--disable-libunwind-exceptions \
		--with-gnu-ld \
		--enable-threads=posix \
		--disable-multilib \
		--disable-libgcj \
		--enable-languages=c,c++

	echo "== make -j$JOBS"
	make -j"$JOBS" LIBPATH="$LIBPATH" STAGE1_CFLAGS="-O"

	echo "== make install"
	make install
}

# ---------------------------------------------------------------- verify ---
do_verify() {
	export PATH=$BINPATH:$PATH
	local want='gcc (GCC) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)'
	local got
	got=$(gcc --version | head -n 1)
	echo "want: [$want]"
	echo "got : [$got]"
	[ "$got" = "$want" ] || { echo "!! VERSION STRING MISMATCH" >&2; exit 1; }
	got=$(g++ --version | head -n 1)
	[ "$got" = "g++ (GCC) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)" ] \
		|| { echo "!! g++ version string mismatch: [$got]" >&2; exit 1; }

	# The C++ half must WORK, not merely exist: a stage that built cc1 and no
	# libstdc++ passes every version test and then fails 31 of 183 objects.
	printf 'int f(int x){return x*3;}\n' > /tmp/t.c
	gcc -c -O2 -march=i386 -o /tmp/t.o /tmp/t.c
	printf '#include <cstddef>\n#include <cmath>\nstruct S{int a;};int g(S*s){return s->a;}\n' > /tmp/t.cpp
	g++ -c -O2 -march=i386 -fno-exceptions -fno-rtti -o /tmp/tpp.o /tmp/t.cpp
	echo -n "== .comment stamped on a C object:  "
	strings -a /tmp/t.o | grep GCC:
	echo -n "== .comment stamped on a C++ object: "
	strings -a /tmp/tpp.o | grep GCC:
	ls -l "$LIBPATH"/libstdc++.a
}

case "${1:-all}" in
unpack)		do_unpack ;;
compile)	do_compile ;;
verify)		do_verify ;;
all)		do_unpack; do_compile; do_verify ;;
*)		echo "usage: $0 [unpack|compile|verify|all]" >&2; exit 1 ;;
esac

# ------------------------------------------------- DELIBERATE DEVIATIONS ---
#
# Each of these is a thing the ebuild does that this script does not, with the
# reason.  Nothing here is an oversight; if one of them turns out to matter it
# is a finding, not a bug fix.
#
#  * `make`, not `profiledbootstrap` (eclass:1011, x86's default target).  The
#    C compiler that builds cc1 does not change what cc1 emits, so three
#    profiled stages would cost several times the wall clock to re-verify a
#    property nothing here depends on.  Finding F2200 made the same call for the
#    stock build and this keeps the two images comparable.
#
#  * --build and --target are spelled out; the eclass passes only --host and
#    lets config.guess find the rest.  It cannot here: `--platform linux/386`
#    gives a 32-bit userland on the host's own 64-bit kernel, so `uname -m`
#    says x86_64 and config.guess configures a compiler for a target that
#    cannot be built.  On a real 2005 box the guess WOULD have returned CHOST.
#
#  * CHOST is i386-pc-linux-gnu, which is this stage3's own (its /etc/make.conf
#    says so), not the i686-pc-linux-gnu that Dockerfile.exact forces.  It sets
#    a DEFAULT -march/-mtune only, and period.mk passes both explicitly.
#
#  * --disable-nls where a default 2005.0 profile would have had USE=nls.
#    Diagnostics language only; it cannot reach code generation, and the stock
#    image disables it too, so the two stay comparable.
#
#  * elibtoolize / gnuconfig_update (eclass:723-725) are portage plumbing for
#    .la files and for updating config.guess -- the latter moot given the
#    forced triplet.
#
#  * split_out_specs_files (eclass:527) writes vanilla/hardened/hardenednossp
#    specs FILES for gcc-config to select between.  make_gcc_hard never runs,
#    so the compiler's built-in specs are the vanilla ones already: shipping
#    the extra files would change nothing about what gets compiled.
#
#  * disable_multilib_libjava (eclass:636) -- libgcj is disabled outright.
#
#  * The whole of src_install's file shuffling (ebuild:185): renaming
#    lib/gcc to lib/gcc-lib, the env.d entries, gcc-config.  All of it is
#    Gentoo package management.  This image is only ever used to COMPILE.
