# test/experiment/Makefile -- the V.90 DIGITAL TERMINATION experiment.
#
# RUN IT FROM THE REPOSITORY ROOT:
#
#     make -f test/experiment/Makefile          # build and run
#     make -f test/experiment/Makefile build    # build only
#
# ===========================================================================
# WHY THIS IS A SEPARATE MAKEFILE AND NOT A TARGET IN THE ROOT ONE
# ===========================================================================
#
# The root Makefile's `TESTS`/`CXXTESTS` are `$(wildcard test/unit/t_*.c)` and
# `test/unit/t_*.cpp`, so ANY file dropped into `test/unit/` joins `make phase`
# automatically.  This binary must NOT: it is a bring-up probe of a code path
# the vendor's object never executes, it is EXPECTED to crash on some of its
# arms, and a crash here is a RESULT rather than a regression.  `test/interop/`
# is named file by file in the root Makefile, so it is closed too, but it is
# also 64-bit and links no blob -- and the blob is exactly what this experiment
# needs on the other side of every comparison.
#
# `test/experiment/` is invisible to the root Makefile: nothing globs it, and
# `build-exp/` is not `build/`, `build/repro/` or `build-cov/`, which is the
# other half of the isolation.  Findings 3055, 3110 and 3111 are seven tools
# broken at once by objects appearing in a directory they globbed; putting
# ours anywhere under `build/` would invite the eighth.
#
# ===========================================================================
# THE FLAGS ARE HARVESTED, NOT RESTATED
# ===========================================================================
#
# Every flag below comes out of the root Makefile through its own `print-%`
# target.  Restating `-nostdinc++ -fno-lifetime-dse -mfpmath=387 ...` by hand
# and getting one of them wrong would make this experiment a measurement of
# this file rather than of the reconstruction, and the tree's own rule about
# one source for a link line (the `.ldflags` note in the root Makefile) says
# so in the case it has already been bitten by.
#
# ===========================================================================
# IT LINKS THE `repro` TREE, WHICH IS THE FAITHFUL ONE
# ===========================================================================
#
# `$(OBJ_REPRO)` is `src/` built with -DDSPLIB_REPRODUCE_BUGS, i.e. the
# reconstruction of the ORIGINAL rather than of our fixed variant.  The
# question this experiment asks is about the vendor's branch, so the vendor's
# behaviour is the one to link.  Checked rather than assumed: no file under
# `src/pump/v90/` carries a `DSPLIB_REPRODUCE_BUGS` site except `V92CP.cpp`,
# which is not in any path this binary drives -- so the two builds are in fact
# identical over this experiment's surface and the choice is documentation.

ROOT_MK     := Makefile
EXPBUILD    := build-exp

CC          := $(shell $(MAKE) -s -f $(ROOT_MK) print-CC)
CXX         := $(shell $(MAKE) -s -f $(ROOT_MK) print-CXX)
ARCH32      := $(shell $(MAKE) -s -f $(ROOT_MK) print-ARCH32)
FPFLAGS     := $(shell $(MAKE) -s -f $(ROOT_MK) print-FPFLAGS)
CXXFLAGS    := $(shell $(MAKE) -s -f $(ROOT_MK) print-CXXFLAGS)
LDFLAGS     := $(shell $(MAKE) -s -f $(ROOT_MK) print-LDFLAGS)
REPRODUCE   := $(shell $(MAKE) -s -f $(ROOT_MK) print-REPRODUCE)
OBJ_REPRO   := $(shell $(MAKE) -s -f $(ROOT_MK) print-OBJ_REPRO)
HARNESS_OBJ := $(shell $(MAKE) -s -f $(ROOT_MK) print-HARNESS_OBJ)
REF         := $(shell $(MAKE) -s -f $(ROOT_MK) print-REF)

SRC         := test/experiment/x_v90digital.cpp
BIN         := $(EXPBUILD)/x_v90digital

.PHONY: all build run objects denominator

all: run

# THE DENOMINATOR, and it is here for finding 2400's reason: a link line whose
# object list came back EMPTY from a `print-` target that had been renamed
# would link a binary of nothing but the harness, and every probe below would
# report "constructed: no" -- which reads exactly like a real negative result.
# Refuse instead, and say what was counted.
denominator:
	@n=$$(echo $(OBJ_REPRO) | wc -w); \
	 h=$$(echo $(HARNESS_OBJ) | wc -w); \
	 echo "experiment: linking $$n reconstruction objects, $$h harness"\
	      "objects and $(REF)"; \
	 test "$$n" -ge 50 || { \
	     echo "experiment: REFUSING -- OBJ_REPRO came back with $$n"\
	          "objects, which is not a tree.  Check print-OBJ_REPRO in"\
	          "the root Makefile."; exit 1; }; \
	 test "$$h" -ge 3 || { \
	     echo "experiment: REFUSING -- HARNESS_OBJ came back with $$h"\
	          "objects."; exit 1; }

objects: denominator
	@$(MAKE) --no-print-directory -f $(ROOT_MK) \
	    $(OBJ_REPRO) $(HARNESS_OBJ) $(REF)

build: objects
	@mkdir -p $(EXPBUILD)
	$(CXX) $(ARCH32) $(FPFLAGS) $(CXXFLAGS) $(REPRODUCE) -Itest/harness \
	    -c $(SRC) -o $(EXPBUILD)/x_v90digital.o
	$(CC) $(ARCH32) $(LDFLAGS) -o $(BIN) $(EXPBUILD)/x_v90digital.o \
	    $(OBJ_REPRO) $(HARNESS_OBJ) $(REF) -lm

run: build
	./$(BIN)

clean:
	rm -rf $(EXPBUILD)
