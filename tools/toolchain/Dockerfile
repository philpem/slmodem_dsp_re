# NOT THE DEFAULT ANY MORE.  `Dockerfile.exact` bootstraps GCC 3.4.2 itself
# from the GNU tarball and is what `build.sh` and `period.sh` reach for; this
# one builds `dsplibs-tc`, sarge's 3.4.4 prerelease, and is kept because it is
# the OTHER ARM of the measurement -- `TC_IMAGE=dsplibs-tc` and
# `PERIOD_IMG=dsplibs-tc` still select it.  The point release is worth 6
# identical functions and 18 of 183 translation units; finding 2200.
#
# What follows was written when this was the only image, and the paragraph
# about sarge bootstrapping an exact 3.4.2 has now been acted on.
#
# The original's toolchain, as closely as it can be had.
#
# `.comment` in dsplibs.o says, 279 times over:
#
#     GCC: (GNU) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)
#
# and the __DATE__ stamps put the build at 22 September 2005.  Finding 346.
#
# WHY DEBIAN SARGE AND NOT GENTOO.  Sarge is the contemporary distribution that
# still has an installable archive: it ships gcc-3.4.3 and binutils 2.15, which
# is the right binutils generation, and it can bootstrap an exact GCC 3.4.2
# from the GNU tarball.  Reconstructing a 2005 Gentoo stage3 is possible but
# buys nothing here: the two Gentoo-specific patchsets named in the version
# string are `ssp` and `pie`, and the object shows BOTH FEATURES OFF -- no
# `__guard` symbol and not one `get_pc_thunk`.  What those patches change when
# they are not enabled is the spec file, not the optimiser.
#
# The image is only ever used to COMPILE, never to run anything: the artefacts
# are `.o` files for instruction-level comparison against the blob.
#
#   docker build --platform linux/386 -t dsplibs-tc tools/toolchain
#   docker run --rm --platform linux/386 -v "$PWD:/src" -w /src dsplibs-tc \
#          gcc-3.4 -c -O2 -march=i386 -mfpmath=387 -fno-pie ...
#
FROM --platform=linux/386 debian/eol:sarge

# archive.debian.org has no valid-until, and sarge's keys are long expired.
RUN printf '%s\n' \
      'Acquire::Check-Valid-Until "false";' \
      'APT::Get::AllowUnauthenticated "true";' \
      > /etc/apt/apt.conf.d/99archive \
 && apt-get update \
 && apt-get install -y --force-yes \
      gcc-3.4 g++-3.4 binutils make libc6-dev \
 && rm -rf /var/lib/apt/lists/*

# `gcc`/`g++` unversioned point at 3.4 so an unmodified Makefile picks it up.
RUN ln -sf /usr/bin/gcc-3.4 /usr/bin/gcc \
 && ln -sf /usr/bin/g++-3.4 /usr/bin/g++ \
 && ln -sf /usr/bin/cpp-3.4 /usr/bin/cpp

CMD ["/bin/sh"]
