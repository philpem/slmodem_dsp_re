#
# THE PERIOD-TOOLCHAIN BUILD.  Compile every reconstructed translation unit
# with GCC 3.4.2 and leave the objects where `compare.py`, `byteident.py`,
# `samesize.py`, `extcheck.py`, `storeorder.py`, `instrcount.py`,
# `eqtriage.py` and `sibcensus.py` look for them.
#
#     make -f tools/toolchain/period.mk            # from the repository root
#     make tc                                      # the same, via the top level
#
# See `Dockerfile.exact` for what the toolchain is and `compare.py` for where
# each flag came from.  Nothing here touches `build/repro` -- the project's own
# build is untouched and this output is for comparison only.
#
# WHY THIS IS A MAKEFILE AND NOT `build.sh` ANY MORE
#
# The shell script rebuilt all 205 objects, serially, in one container, on
# every run -- because `rm -rf $OUT` was the only dependency tracking it had.
# Editing one file cost a full rebuild, so the tools that read `build/tc_out`
# grew a staleness guard (`byteident.py::_staleness`) whose docstring says the
# quiet part: "no Makefile rule depends on it, so any change to `src/` leaves
# it behind while every count here keeps rendering as a clean, plausible,
# WRONG number".  That is finding F2400's shape, and the cure for it is
# dependencies, which is what this file adds:
#
#   - one rule per object, so one edited source recompiles one object;
#   - REAL header dependencies from `-MMD -MP`, so editing a header rebuilds
#     exactly the objects that include it -- `period_inner.sh` still has to
#     approximate this with "newer than the newest header by mtime", and says
#     so at its line 42;
#   - `-j`, which the script could not use at all.
#
# ONE `docker run` PER OBJECT, AND THAT IS MEASURED, NOT ASSUMED.  On this
# machine `docker run --rm ... true` costs 0.55 s and `docker exec` into an
# already-running container costs 0.07 s, so a persistent container looks
# eight times cheaper.  It is not, for the case that matters: the incremental
# build compiles one object, and `exec` has to start the container first, so
# `run` (0.55 + 0.25) beats `start + exec` (0.55 + 0.07 + 0.25) outright.  A
# persistent container only wins the FULL rebuild, and buys that with daemon
# state that outlives make, a name race under `-j`, and a container left
# running when make is interrupted.  Not worth it.  A full rebuild is 205
# containers; run it under `-j`.
#

ifeq ($(wildcard src),)
$(error run this from the REPOSITORY ROOT: make -f tools/toolchain/period.mk)
endif

.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables

#
# UNDER build/, NOT UNDER /tmp, so `make clean` reaches it.  This defaulted to
# /tmp/tc_out and wrote its manifest to /tmp/tc_manifest.txt -- 152 objects and
# an index that nothing in the tree ever removed, and that two concurrent
# worktrees would have written over each other.
#
TC_OUT   ?= $(CURDIR)/build/tc_out

# TWO KNOBS, BOTH FOR A/B MEASUREMENT AND NEITHER A WAY TO CHANGE THE BUILD.
#
#   TC_IMAGE=dsplibs-tc      the OLD image, Debian sarge's GCC 3.4.4; the
#                            default is `dsplibs-tc342`, GCC 3.4.2 itself
#                            (Dockerfile.exact).  Finding F2200
#   TC_IMAGE=dsplibs-tc342
#                            stock GCC 3.4.2, retained as an A/B arm.
#   TC_IMAGE=dsplibs-tc342-gentoo
#                            the default and only exact arm: Gentoo's
#                            gcc-3.4.2-r2, built from the ebuild inside
#                            stage3-x86-2005.0, printing the blob's .comment
#                            back byte for byte (Dockerfile.gentoo).  It is
#                            NOT the default because it needs a stage3 and
#                            28 MB of distfiles that are not in git -- and it
#                            costs nothing to skip: 182 of 183 objects come
#                            out byte-identical to the default's, and the
#                            symbol match is 334 either way.  Finding F2500
#   TC_EXTRA="-O2"           APPENDED after the flags, so a repeat of an option
#                            overrides the one above -- `-O2` beats the `-O3`,
#                            `-mieee-fp` beats the `-mno-ieee-fp`.  That is how
#                            finding F2200's arms were taken without editing
#                            this file, which is what a flag conclusion has to
#                            be measured against.
#
# You NO LONGER have to set TC_OUT as well.  Under `build.sh` the arms shared
# one output directory and `rm -rf` was what kept them apart, so forgetting it
# compared one arm against another arm's leftovers.  Here the image and the
# whole flag string are a PREREQUISITE (see TC_STAMP): change either and every
# object rebuilds.  Findings F2155 and F1990 are the flag record; this is still
# not a supported way to build the tree differently from what they say.
#
TC_IMAGE ?= dsplibs-tc342-gentoo
TC_EXTRA ?=

# `-mno-ieee-fp` IS IN `make period` TOO NOW, so the two sets are identical
# again.  It used to be here only: the object's float compares are ordered --
# 406 against four, and those four are inside libm -- and with the flag the
# period tier went 181 passed to 176, failing five suites on NaN and near-NaN
# inputs.  Finding F1990 refused to say whether that meant five defects or a
# wrong flag; findings F2300 to F2303 say it was five defects, and the tier is
# green with the flag.
#
# THE SAME FLAGS `make period` USES, and they must stay the same.  The two
# diverged once and it cost real coverage: this build passed neither
# -D__SIZEOF_POINTER__=4 nor the compat header, so it compiled a smaller set
# than the period differential AND silently elided the 81 offset assertions
# guarded on that predefine -- V3 in docs/method/compilers.md, the variance
# that fails OPEN.  `tools/assertlive.py` names THIS FILE and
# `period_inner.sh` as the two that must carry the -D, and checks both.
# The remaining duplication is the flag string itself, still spelled twice;
# `period_inner.sh:23` is the other home.
#
# -std=gnu99 is NOT here and is not an oversight: it is the C dialect the
# TEST HARNESS needs, and this build compiles only src/.
#
TC_FLAGS := -O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 \
            -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args \
            -Iinclude -D__SIZEOF_POINTER__=4 \
            -include tools/toolchain/period_compat.h
TC_FLAGS += $(TC_EXTRA)

# DCR's recovered GCC preimage is O2 without the post-loop CSE rerun.  Its
# instruction graph matches the blob under live-range renaming; the only
# remaining difference is the compiler's 0x5c versus 0x2c frame reservation.
# This is deliberately source-specific: changing the global level would move
# hundreds of unrelated functions.  Finding F10217 records the evidence.
TC_DCR_FLAGS := -O2 -fno-rerun-cse-after-loop

# Appended AFTER $(TC_EXTRA), exactly as `build.sh` ordered them, so the
# override semantics above are unchanged.
TC_CXXONLY := -fno-exceptions -fno-rtti

#
# THE SOURCE LIST HAS ONE HOME AND IT IS THE TOP-LEVEL MAKEFILE.  Re-deriving
# it here with `find` would be a second definition of the same set, which is
# this tree's most repeated bug in its other form (`onedef.py`).
#
# MAKEFLAGS is cleared and the directory banner suppressed: run from inside a
# make recipe -- which is now the normal case, `make tc` -- both leak
# `make[1]: Entering directory ...` and a jobserver warning into the variable,
# and the container then tries to compile them.
#
TC_SRC    := $(shell MAKEFLAGS= $(MAKE) -s --no-print-directory print-SRC)
TC_CXXSRC := $(shell MAKEFLAGS= $(MAKE) -s --no-print-directory print-CXXSRC)

# A DETECTOR MUST REPORT ITS DENOMINATOR, and a build is one.  If the shell-out
# above fails -- wrong directory, a syntax error in the top-level Makefile, a
# `print-%` rule that stops printing -- the lists come back EMPTY, every rule
# below is generated over nothing, the object count assertion compares 0
# against 0, and this exits 0 having compiled nothing.  Findings F2400, F2401,
# F3110.
ifeq ($(strip $(TC_SRC)$(TC_CXXSRC)),)
$(error the source list came back EMPTY from `$(MAKE) print-SRC` -- nothing \
        would be compiled and this build would exit 0 having measured nothing)
endif

#
# OBJECT NAME <- SOURCE PATH, FORWARD ONLY.  The name is the full path with
# slashes turned into underscores, extension and all:
#
#     src/pump/v34/V34hshak.c   ->   src_pump_v34_v34hshak.c.o
#
# It is NOT reversible -- `src/core/dp_wrapper.c` and a directory called `dp`
# produce the same string -- so there is no pattern rule that could express it
# and none is attempted.  Each rule is generated from its source, and the
# mapping is RECORDED in tc_manifest.txt rather than guessed at by anyone who
# needs to go the other way (`compare.py:346`).
#
tcobj = $(TC_OUT)/$(subst /,_,$(1)).o

TC_OBJ      := $(foreach s,$(TC_SRC) $(TC_CXXSRC),$(call tcobj,$(s)))
TC_MANIFEST := $(TC_OUT)/tc_manifest.txt
TC_STAMP    := $(TC_OUT)/.build-config
TC_GCCVER   := $(TC_OUT)/.gcc-version
TC_DEPDIR   := $(TC_OUT)/.deps

# Objects and dep files with no source any more -- a class `rm -rf $OUT` made
# impossible and incremental building brings back.  Removed by the `tc` recipe
# before it counts, so a deleted source cannot leave a symbol behind for nine
# globbing tools to keep scoring.
TC_STALE   := $(filter-out $(TC_OBJ),$(wildcard $(TC_OUT)/*.o))
TC_STALE_D := $(filter-out $(patsubst $(TC_OUT)/%,$(TC_DEPDIR)/%.d,$(TC_OBJ)), \
                           $(wildcard $(TC_DEPDIR)/*.d))

# The recovered Gentoo driver calls `whoami`, so it cannot run as this host's
# unrecorded bind-mount UID.  Build outputs are ignored and their directory is
# user-writable, so root-owned objects remain removable between configurations.
ifeq ($(TC_IMAGE),dsplibs-tc342-gentoo)
TC_RUN := docker run --rm --label dsplibs-tc --platform linux/386 \
	  -v '$(CURDIR):/src' -v '$(TC_OUT):/out' -w /src $(TC_IMAGE)
else
TC_RUN := docker run --rm --label dsplibs-tc \
	  --user $(shell id -u):$(shell id -g) --platform linux/386 \
	  -v '$(CURDIR):/src' -v '$(TC_OUT):/out' -w /src $(TC_IMAGE)
endif

.DEFAULT_GOAL := tc
.PHONY: tc tc-image tc-reap tc-clean FORCE

#
# THE DEFAULT GOAL ASSERTS ITS OWN COMPLETENESS.  A partial `build/tc_out` is
# a state `build.sh` could not produce and this file can: nine tools glob that
# directory, and every one of them would happily score whatever survived a
# failed build and report a clean, plausible number.  So a compile error stops
# the build (the recipes do NOT send stderr to /dev/null, which `build.sh`
# did), and the count is checked against the source list before the summary
# line is allowed to print.  Findings F2400 and F3100.
#
tc: $(TC_OBJ) $(TC_GCCVER)
	@rm -f $(TC_STALE) $(TC_STALE_D)
	@for f in $(TC_SRC) $(TC_CXXSRC); do \
	   echo "$$(echo $$f | tr / _).o $$f"; \
	 done > '$(TC_MANIFEST).new'
	@if cmp -s '$(TC_MANIFEST).new' '$(TC_MANIFEST)'; then \
	   rm -f '$(TC_MANIFEST).new'; \
	 else mv '$(TC_MANIFEST).new' '$(TC_MANIFEST)'; fi
	@n=$$(ls '$(TC_OUT)'/*.o 2>/dev/null | wc -l); want=$(words $(TC_OBJ)); \
	 if [ "$$n" -ne "$$want" ]; then \
	   echo "period toolchain: $$n objects in $(TC_OUT), expected $$want."; \
	   echo "  The tree is PARTIAL, and every tool that reads it globs *.o"; \
	   echo "  and would score exactly what survived.  Findings F2400, F3100."; \
	   exit 1; \
	 fi; \
	 echo "period toolchain: $$n objects from $$want sources, 0 failed, gcc $$(cat '$(TC_GCCVER)') [$(TC_IMAGE)]"

#
# THE IMAGE AND THE FLAG STRING ARE A PREREQUISITE.  `rm -rf $OUT` used to do
# this work by brute force.  Without it, `TC_EXTRA=-O2 make -f period.mk` over
# an up-to-date tree recompiles NOTHING and then compares -O3 objects in the
# belief that they are -O2 -- and findings F2155, F1990 and F2200 all rest on
# that comparison being clean.  The stamp is content-compared, so it is only
# touched when something really changed and an unchanged run rebuilds nothing.
#
FORCE:

$(TC_STAMP): FORCE | $(TC_OUT) tc-image
	@printf 'image %s\nflags %s\ncxx   %s\ndcr   %s\n' \
	        '$(TC_IMAGE)' '$(TC_FLAGS)' '$(TC_CXXONLY)' '$(TC_DCR_FLAGS)' > '$@.new'
	@if cmp -s '$@.new' '$@'; then rm -f '$@.new'; else \
	   mv '$@.new' '$@'; \
	   echo "  TC-CFG  $(TC_IMAGE) $(if $(TC_EXTRA),TC_EXTRA=$(TC_EXTRA) ,)-- rebuilding every object"; \
	 fi

# Cached, because it costs a container and the summary line prints it on every
# run.  Refreshed exactly when the image or the flags change.
$(TC_GCCVER): $(TC_STAMP)
	@$(TC_RUN) gcc -dumpversion > '$@'

$(TC_OUT) $(TC_DEPDIR):
	@mkdir -p '$@'

tc-image:
	@docker image inspect '$(TC_IMAGE)' >/dev/null 2>&1 || { \
	   echo "tools/toolchain: no docker image '$(TC_IMAGE)'.  Build it with" >&2; \
	   echo "  docker build --platform linux/386 \\" >&2; \
	   echo "    -f tools/toolchain/Dockerfile.exact -t dsplibs-tc342 tools/toolchain" >&2; \
	   echo "(the older 3.4.4 image is Dockerfile, -t dsplibs-tc.  Finding F2200.)" >&2; \
	   exit 1; }

#
# `-MT '$@'` IS LOAD-BEARING AND ITS ABSENCE IS SILENT.  GCC names the rule in
# the dep file after the `-o` argument, which inside the container is
# `/out/src_x.c.o`; make knows that object as `$(TC_OUT)/src_x.c.o`.  Without
# `-MT` the two never match, every generated rule is inert, and NOTHING gives
# it away -- the `.d` files exist, the build succeeds, and header edits are
# quietly ignored for ever.
#
# GCC 3.4.2's `-MT` ADDS a target rather than replacing the `-o`-derived one,
# so each `.d` also carries a rule for `/out/src_x.c.o`.  That target is never
# a goal and never a prerequisite, so it is inert; it is left rather than
# post-processed away, because a sed over generated makefiles is a second
# place for this to go wrong.
#
# The deps live in `.deps/` and not beside the objects.  Every tool that reads
# this directory globs `*.o` -- checked, all nine -- so `.d` files beside them
# would be harmless today and a trap for the first tool that lists instead.
#
define TC_CC_RULE
$(call tcobj,$(1)): $(1) $$(TC_STAMP) | $$(TC_DEPDIR)
	@echo '  TC-CC   $(1)'
	@$$(TC_RUN) gcc -c $$(TC_FLAGS) $(if $(filter src/service/dcr.c,$(1)),$(TC_DCR_FLAGS)) -MMD -MP -MT '$$@' \
	    -MF '/out/.deps/$$(@F).d' -o '/out/$$(@F)' '$$<'
endef

define TC_CXX_RULE
$(call tcobj,$(1)): $(1) $$(TC_STAMP) | $$(TC_DEPDIR)
	@echo '  TC-CXX  $(1)'
	@$$(TC_RUN) g++ -c $$(TC_FLAGS) $$(TC_CXXONLY) -MMD -MP -MT '$$@' \
	    -MF '/out/.deps/$$(@F).d' -o '/out/$$(@F)' '$$<'
endef

$(foreach s,$(TC_SRC),$(eval $(call TC_CC_RULE,$(s))))
$(foreach s,$(TC_CXXSRC),$(eval $(call TC_CXX_RULE,$(s))))

-include $(wildcard $(TC_DEPDIR)/*.d)

#
# `--rm` cleans up a container that exits, including one killed by the ^C make
# forwards.  `tc-reap` is for the case it does not -- the label is on every
# container this file starts, so it needs no bookkeeping and cannot match
# anything else on the machine.  `build.sh` used a `--name` and a shell trap
# for the same job; with one container per object there is no trap to hang it
# on, and a label is the handle that survives.
#
tc-reap:
	@ids=$$(docker ps -aq --filter label=dsplibs-tc); \
	 if [ -n "$$ids" ]; then docker rm -f $$ids; else echo "no dsplibs-tc containers"; fi

tc-clean:
	rm -rf '$(TC_OUT)'
