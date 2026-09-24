# The modern (portability) tier's compiler, pinned so verdicts reproduce.
#
# WHY THIS EXISTS.  A mutation verdict is a property of the SOURCE, the TEST
# and the COMPILER that built them, but `mutsnap.py`'s key covers only the
# source.  So a re-record made under a different GCC is not comparable with
# one made under another, and the tree wants ONE modern compiler rather than
# whatever the host happens to ship.
#
# THE ONE IS GCC 13.  `docs/method/compilers.md` ("Moving from GCC 13 to GCC
# 3.4.2") and every entry in `tools/gccdiverge.json` are written against GCC
# 13, and the historical modern-tier fixture (issue #96) recorded exactly
# `gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`.  GCC 14 diverges (issues
# #30/#172), so it is not the reference.
#
# Ubuntu 24.04 LTS ships that 13.3.0 and binutils 2.42 as its stock packages,
# which is why this is `ubuntu:24.04` with no PPA or version pin: the base
# image IS the pin, and rebuilding it later reproduces the same compiler.
#
# `gcc-multilib`/`g++-multilib` are required because the modern build is
# `-m32 -mfpmath=387`.
FROM ubuntu:24.04

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        gcc-multilib \
        g++-multilib \
        python3 \
        git \
        ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# A banner so a run says which compiler produced its verdicts.
RUN gcc --version | head -1
