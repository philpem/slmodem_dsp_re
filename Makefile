###########################################################################
#
#	dsplibs -- reconstruction of slmodemd/dsplibs.o
#
#	Self-contained: this tree never builds from, or writes into,
#	../slmodemd or ../re.  The only thing it reads outside itself is
#	the reference object ../slmodemd/dsplibs.o.
#
###########################################################################

#
# OVERRIDABLE, so a worktree anywhere can build.  `git worktree add ../name`
# puts the tree beside this one and the relative path resolves, which is why
# sibling worktrees have always worked; a worktree somewhere else does not,
# and one session serialised its whole run believing that ruled out parallel
# trees entirely.  `make BLOB=/abs/path/dsplibs.o` now covers both.
#
BLOB       ?= ../slmodemd/dsplibs.o
#
# EXPORTED, because the recipes are not the only thing that opens it.
# `tools/debugaudit.py` and `tools/coverage.py` are invoked with no path and
# default to the sibling one, so `make BLOB=/abs/path phase` used to fail at
# `strings` in any worktree that is not beside slmodemd -- the override
# covered the recipes and not the tools they run.  Both now read $BLOB with
# the same default, and `debugcov`'s sub-make inherits it too.
#
export BLOB

#
# PARALLEL BY DEFAULT, because nobody remembers the flags.
#
# Every object, every test binary and every test RUN is independent -- each
# test links its own copy of everything and touches no shared file -- so
# there is nothing here that wants to be serial.  Measured from clean on 12
# cores:
#
#     make test    27.7 s  ->   6.4 s
#     make phase   75.2 s  ->  26.6 s
#
# `phase` is the inner loop; a session runs it ten to twenty times per
# function, so this is minutes per function rather than seconds.
#
# --output-sync=target groups each recipe's output, or 77 tests interleave
# their PASS lines into nonsense.  It needs GNU make 4.0, and the spelling is
# `target`; `recipe` is not a sync type and make rejects it outright.
#
# `make J=1` for a serial build when a failure needs reading in order.
#
J          ?= $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS  += -j$(J) --output-sync=target
BUILD      := build

# 32-bit is forced by the reference object, not by our own code -- the
# reconstruction is written 64-bit-clean.  `make check64` proves it still
# compiles for a native target; it just cannot be differential-tested there.
ARCH32     := -m32
CC         := gcc
CXX        := g++
#
# Header-dependency generation, kept separate so it can be filtered back out.
# `check64` compiles with -fsyntax-only and no -o, and GCC then writes each
# .d beside the Makefile rather than into $(BUILD) -- 62 stray files in the
# working tree after every `make phase`.  They are gitignored, so this was
# untidiness rather than breakage, but the fix is to not emit them.
#
DEPFLAGS   := -MMD -MP
CFLAGS     := -fno-pie -fno-stack-protector -Wall -Wextra -Wno-unused-parameter -g -O2 -Iinclude $(DEPFLAGS)

# The same flags without the dependency generation, for syntax-only passes.
SYNCFLAGS   = $(filter-out $(DEPFLAGS),$(CFLAGS))
SYNCXXFLAGS = $(filter-out $(DEPFLAGS),$(CXXFLAGS))

# Bug-compatibility switch.
#
# The reconstruction fixes defects found in the original ONLY where leaving
# them in would break a working modem, and every such fix is behind this
# define so bit-exactness against the blob remains provable.  Currently one:
# D4, FPM_div's out-of-range table read, which silences an AGC block and drops
# a Bell 103 call (finding 40).
#
# The differential tier defines it -- those tests exist to prove equivalence
# and cannot do that against a fixed build.  Everything else, including the
# interop tier and anyone linking this library for real, gets the fix.
REPRODUCE  := -DDSPLIB_REPRODUCE_BUGS
PYTHON     := python3

# x87 with 80-bit intermediates, matching GCC 3.4.2's -m32 default and the
# blob's own code generation (5300 x87 instructions, zero SSE).  Do not
# change this without re-reading docs/findings.md section 8.
FPFLAGS    := -mfpmath=387

#
# WILDCARDED, all four, because each was a single line that every parallel
# agent had to edit.  Two of them collided in one merge; the CXXTESTS line
# alone was touched by eight batches in one session.  A directory listing
# cannot collide.  Checked equal to the explicit lists they replace -- 67
# sources, 9 C++ sources, 68 tests, 9 C++ tests -- so this is not a change
# in what gets built, only in who has to remember to add to it.
#
SRC        := $(shell find src -name '*.c' | sort)
CXXSRC     := $(shell find src -name '*.cpp' | sort)
OBJ        := $(patsubst %.c,$(BUILD)/%.o,$(SRC)) \
              $(patsubst %.cpp,$(BUILD)/%.o,$(CXXSRC))

# The original was built -fno-exceptions -fno-rtti with no new/delete (zero
# __cxa_*, _Unwind_* or _ZTI* references -- docs/findings.md section 2), so
# match it.
#
# Deliberately NOT -ffloat-store: the original accumulates at 80-bit extended
# precision and rounds once at the end, and -ffloat-store would additionally
# round every intermediate product.  See src/dsp/FloatIIR.cpp.
# -nostdinc++ keeps <stdio.h> and friends resolving to the plain C headers.
# We use none of the C++ standard library (nor did the original), and the
# 32-bit libstdc++ headers are typically absent on a 64-bit host.
CXXFLAGS   := $(CFLAGS) -fno-exceptions -fno-rtti -nostdinc++

# v34hsstep.c is the per-dispatch-case fixture for `v34handshak`.  It lives
# here rather than inside one test file because #56-#58 are sixteen tests over
# the same object, and a fixture each of them copies is a fixture sixteen of
# them will drift.  It costs every other binary some .bss and nothing else.
HARNESS    := test/harness/harness.c test/harness/runtime.c \
              test/harness/fakedp.c test/harness/v34hsstep.c
HARNESS_OBJ:= $(patsubst %.c,$(BUILD)/%.o,$(HARNESS))

TESTS      := $(basename $(notdir $(wildcard test/unit/t_*.c)))
CXXTESTS   := $(basename $(notdir $(wildcard test/unit/t_*.cpp)))
TESTBIN    := $(addprefix $(BUILD)/test/,$(TESTS) $(CXXTESTS))

REF        := $(BUILD)/dsplibs_ref.o
GLOBALS    := $(BUILD)/globals.txt
SYMMAP     := $(BUILD)/symmap.txt

# dsplibs.o predates modern hardening defaults: it wants an executable stack
# and has relocations in .rodata, which a PIE link cannot satisfy.  Both are
# properties of the 2003 reference object, not of anything we build, so relax
# them for test binaries only.
LDFLAGS    := -no-pie -Wl,-z,noexecstack,-z,notext

.PHONY: firewall strings offsets refs all test check64 docs clean interop capture coverage debugcov phase

# Keep intermediates: chained implicit rules otherwise delete them, forcing a
# full rebuild on every invocation.
.SECONDARY:

all: $(TESTBIN)

# --- reference object -----------------------------------------------------
#
# Rename every symbol the blob defines to ref_*, plus the stateful imports.
# tools/symmap.py refuses to run if an import is unclassified, so a future
# blob revision cannot silently gain a shared-state callback.

$(SYMMAP): tools/symmap.py $(BLOB) | $(BUILD)
	$(PYTHON) tools/symmap.py $(BLOB) --globals $(GLOBALS) -o $@

#
# TWO PASSES, because a symbol the object keeps file-local cannot be renamed
# while it is local -- and this tree said for a long time that it therefore
# could not be aliased at all.  It can: --globalize-symbols promotes them,
# then --redefine-syms sees ordinary globals.  That is 241 symbols, 15 of
# them functions already reconstructed here -- call_run, b103_process,
# v23_process, v8_process among them -- which had no ref_ alias and were
# reachable only through a caller.  Ten names are used by more than one
# translation unit and stay local; symmap.py names them each run.
#
$(REF): $(SYMMAP) $(BLOB)
	objcopy --globalize-symbols=$(GLOBALS) $(BLOB) $(BUILD)/dsplibs_glob.o
	objcopy --redefine-syms=$(SYMMAP) $(BUILD)/dsplibs_glob.o $@
	@# The blob's linkonce sections carry the SAME names our objects would
	@# use if built by the period compiler, and linkonce keeps only the
	@# first of a name -- so ours would silently discard the blob's copy and
	@# every ref_ alias inside it.  Finding 349.
	python3 tools/refrename.py $@

# --- compilation ----------------------------------------------------------

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(ARCH32) $(FPFLAGS) $(CFLAGS) $(REPRODUCE) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(ARCH32) $(FPFLAGS) $(CXXFLAGS) $(REPRODUCE) -c $< -o $@

# Linked with $(CC), not $(CXX): the C++ WE have written is -fno-exceptions
# -fno-rtti with no virtuals and no new/delete, so nothing needs libstdc++ --
# which is just as well, since the 32-bit one is often not installed alongside
# a 64-bit toolchain.
#
# This used to add "exactly as the original was built", and that clause was
# wrong about virtuals.  The original has four vtables -- Resampler,
# V90Resampler, ResamplerTiming, ResamplerTimingOffset -- and the dispatch is
# live rather than vestigial: V90Resampler::resample overrides
# Resampler::resample, and Resampler::timingCorrection is a one-byte `ret`,
# an empty base implementation for derived classes to replace.
#
# The rest of the clause stands, and the absence of typeinfo is what proves
# it: zero _ZTI/_ZTS alongside four vtables is exactly what -fno-rtti WITH
# virtual functions emits.  Zero __cxa_*, _Unwind_*, _Znw*, _Zdl*, _ZdaPv too.
# So exceptions, RTTI and new/delete are all still correctly described above,
# and the link still needs no libstdc++.
#
# It matters for layout, not for the build: a virtual class carries a vptr at
# offset 0, so every member of those four sits four bytes further along than a
# non-virtual reading of the disassembly would put it.  A struct that is right
# in size and wrong by four in every offset passes a size check and fails
# everything else.  tools/cppstruct.py flags this without going near a vtable:
# a destructor listed with a D0 variant is a deleting destructor, which GCC
# emits only for a virtual one.  Finding 228.
$(BUILD)/test/%: $(BUILD)/test/unit/%.o $(OBJ) $(HARNESS_OBJ) $(REF)
	@mkdir -p $(dir $@)
	$(CC) $(ARCH32) $(LDFLAGS) -o $@ $^ -lm

$(BUILD)/test/unit/%.o: test/unit/%.c
	@mkdir -p $(dir $@)
	$(CC) $(ARCH32) $(FPFLAGS) $(CFLAGS) $(REPRODUCE) -Itest/harness -c $< -o $@

$(BUILD)/test/unit/%.o: test/unit/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(ARCH32) $(FPFLAGS) $(CXXFLAGS) $(REPRODUCE) -Itest/harness -c $< -o $@

$(BUILD):
	@mkdir -p $(BUILD)

# --- targets --------------------------------------------------------------

#
# THE INNER LOOP.  `make phase` builds and runs 92 binaries; a batch that
# touches three or four of them pays for the other 88 on every iteration, ten
# to twenty times per function.  This builds and runs only what you name:
#
#     make one T=t_v90jd
#     make one T="t_v90jd t_v92jd"
#
# It runs the cheap correctness gates too -- firewall, strings, offsets, refs
# are seconds, and they are the ones that catch a licence leak or an invented
# literal early.  It does NOT run check64, interop, coverage or debugcov.
#
# `make phase` before every commit regardless.  This is for the twenty runs
# between commits, not the one that decides.
#
#
# Ask make what a variable expands to, rather than having tools grep this
# file for it.  debugcov.py and mewtsweep.py both did, with a regex for
# `^TESTS\s*:=(.*)$`, and the moment TESTS became a wildcard expression they
# got the expression as text and tried to build a target called
# `$(basename`.  A tool that parses a Makefile is a tool that breaks when the
# Makefile is edited.
#
#     make print-TESTS
#
print-%:
	@echo '$($*)'

one: firewall strings offsets refs
	@test -n "$(T)" || { echo "usage: make one T=t_name [T=...]"; exit 1; }
	@$(MAKE) --no-print-directory $(addprefix $(BUILD)/test/,$(T))
	@rc=0; for t in $(T); do ./$(BUILD)/test/$$t || rc=1; done; exit $$rc

#
# EVERY TEST IS ITS OWN TARGET, so `make -j` runs them in parallel as well as
# building them in parallel.  A serial shell loop over $(TESTBIN) does not
# care how many cores there are, and the 77 binaries are wholly independent:
# each links its own copy of everything and touches no shared file.
#
# Measured on 12 cores, from clean:
#
#     build   18.5 s serial   ->  3.3 s at -j12
#     run      7.7 s serial   ->  1.4 s at -j12
#
# NO STAMP FILES.  The obvious version records a `.ran` marker so a passing
# test is not re-run, which turns `make test` into something that can report
# success without having executed anything -- the exact failure this tree
# keeps finding in its own checks.  These are phony, so they always run.
#
# Use `--output-sync=recipe` with -j or the PASS lines interleave; `make
# phase` sets it for you.
#
RUNTESTS := $(addprefix run-,$(TESTS) $(CXXTESTS))
.PHONY: $(RUNTESTS)
$(RUNTESTS): run-%: $(BUILD)/test/%
	@./$<

test: firewall strings offsets refs $(RUNTESTS)

# The licence firewall, mechanically.  SpanDSP is LGPL and this tree is BSD,
# and the rule is that no SpanDSP header, source, table or algorithm is
# reachable from src/ -- it is a test peer and nothing else.
#
# Checked as an INCLUDE rather than as the bare word, which is what
# third_party/README.md always meant: a comment may refer to an interop test
# by name, and src/pump/v23/bwchdem.c does exactly that, recording what the
# third party settled about its resonators.  A grep for the word would fail on
# that and teach everyone to ignore the check.
firewall:
	@if grep -rnE '^[ \t]*#[ \t]*include.*spandsp' src/ include/; then \
	    echo "LICENCE FIREWALL BREACHED: the line above includes a SpanDSP header from src/"; \
	    exit 1; \
	fi
	@echo "licence firewall: no SpanDSP include reachable from src/  OK"

# No string in src/ that the object does not also hold.
#
# Same shape as the firewall above and here for the same reason: a policy the
# differential tier is structurally blind to.  Four invented format strings
# survived full differential tests because `dsplibs_debug_level` ships at zero,
# so a wrong string and a right one behave identically (finding 180).  A check
# nobody runs decays into a check that passes because it never ran, which is
# finding 134's own argument, so it runs here rather than on request.
strings:
	@out=`$(PYTHON) tools/debugaudit.py --invented` || { \
	    echo "$$out"; \
	    echo "INVENTED STRING: the lines above are in src/ and not in the blob"; \
	    exit 1; \
	}; \
	echo "$$out" | tail -1

# Every `/* +0xNNN */` in the headers, against what the compiler lays out.
#
# The paddings between fields are stated as absolute spans, so one wrong span
# slides every field after it -- and both spellings compile.  A merge that
# interleaves two branches' fields has to recompute those by hand, which is
# exactly when nothing else in the tree can tell.  See tools/offcheck.py.
offsets:
	@$(PYTHON) tools/offcheck.py

# V90Parameters.h and V92Parameters.h against the blob's own `loadParams`.
#
# Those two headers are LAYOUT ONLY -- not one member is written -- so `test`
# cannot see them at all, and half the constructors in VPcmV34Main.cpp's span
# take a pointer to one.  A later batch moving a field would leave every gate
# in this tree green.  So the object is the oracle directly: tools/vparse.py
# re-reads the 295 + 54 (name, offset, int-or-float) triples out of the two
# `loadParams` members and paramcheck.py compares them with the header text.
# The syntax check is here rather than in `check64` because nothing includes
# either header yet, and an uncompiled header is not a checked one.
params: | $(BUILD)
	@$(PYTHON) tools/paramcheck.py --emit $(BUILD)
	@for h in V90Parameters V92Parameters; do \
	    $(CXX) $(ARCH32) $(CXXFLAGS) -fsyntax-only $(BUILD)/_$$h.cpp || exit 1; \
	    $(CXX) $(SYNCXXFLAGS) -fsyntax-only $(BUILD)/_$$h.cpp || exit 1; \
	done
	@echo "parameter headers: layout matches the object, and the compiler agrees"

# How many diagnostic call sites the suite never reaches.
#
# TRACKED, NOT GATED.  53 of 279 are dead today and retiring them is task #50;
# a check that is red from its first run is a check somebody turns off.  What
# this is for is the trend -- the number should fall at every phase boundary,
# and a RISE means a batch of sites was placed without anything to drive them.
#
# It builds a second, instrumented tree in build-cov/ and runs all 62 binaries
# there, which is why it is last: it doubles the wall clock of `make phase`.
# The one thing it does fail on is an instrumented test disagreeing with the
# blob -- for the ordinary reason, since the goal is a replacement that behaves
# identically and any disagreement is a hard failure whatever build it came
# from.  See finding 192 and tools/debugcov.py.
debugcov:
	@$(PYTHON) tools/debugcov.py --summary

# The prose half of the same job.  `offsets` holds the compiler to the
# /* +0xNNN */ annotations; nothing at all held the `finding N` and `DN`
# citations, and those are renumbered BY HAND every time a merge makes
# room for two sessions' findings.  Only the dangling check belongs here.
# The mode that catches the failure that actually happens -- a reference
# that still resolves, to the wrong entry -- needs a revision to compare
# against, and the revision that matters is a merge parent:
#
#     git log --merges -1 --format=%P | tr ' ' '\n' | \
#         xargs -I{} tools/refcheck.py --since {}
refs:
	@$(PYTHON) tools/refcheck.py
	@$(PYTHON) tools/anchorcheck.py
#
# The mutation snapshot: hashing only, 0.03 s, runs nothing.  It fails on
# MISSING, ORPHANED and INCONSISTENT -- always defects -- and REPORTS
# staleness without failing.  That is deliberate: this target is on the
# path everybody takes, almost any src/ edit makes every entry stale, and a
# gate that is red by default gets ignored while the cheapest way to clear
# it (`--update`) would manufacture a baseline nobody examined.  `--strict`
# adds staleness and is what a MERGE passes, where refreshing is honest.
#
	@$(PYTHON) tools/mutsnap.py --check

# Everything a phase boundary is supposed to check, in one target.
#
# This exists because `make test` and `make interop` link DIFFERENT runtimes,
# so a module added to $(SRC) can build and pass every differential test while
# leaving the interop tier unbuildable.  That is exactly what happened when
# v34filters.c became the first module to import the debug hooks: 546 tests
# passed and `make interop` had been broken for two commits.
#
# Run this at every phase boundary, not `make test`.
phase: test check64 interop params coverage debugcov
	@echo
	@echo "phase boundary: differential, 64-bit, interop, coverage and debug sites all OK"

# SpanDSP interop.  A SEPARATE 64-bit binary: the system SpanDSP is amd64 and
# the blob is i386, so the two tiers cannot share a build.  That is a feature --
# this tier answers "is it a correct Bell 103 modem", which the blob cannot be
# the judge of.  Needs libspandsp-dev.
# THE 64-BIT LINK NEEDS THE C++ HALF OF src/ TOO, since `v34handshak`'s
# microstate arm 51 calls `V34SetINFO1aBits` and that lives in a .cpp.  It is
# the tree's first .c -> .cpp reference and it is the object's, not a choice
# here: 0x6ba1a is a call and inlining the callee instead would put a second
# copy of 1,401 bytes next to the one `t_v34info1a.c` already sweeps.
#
# Objects and not sources on the link line, because `g++` would compile the
# .c half as C++.  No -lstdc++: CXXFLAGS is -fno-exceptions -fno-rtti
# -nostdinc++ and the reconstruction uses no runtime.
CXXOBJ64   := $(patsubst src/%.cpp,$(BUILD)/64/%.o,$(CXXSRC))

$(BUILD)/64/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

INTEROP_SRC := test/interop/t_spandsp_b103.c test/interop/runtime64.c
SPANDSP     := third_party/spandsp
SPANDSP_LIB := $(SPANDSP)/src/.libs/libspandsp.a

# Freeze a SpanDSP signal so the 32-bit differential harness can replay it.
capture: $(BUILD)/capture/spandsp_b103.pcm

$(BUILD)/capture/spandsp_b103.pcm: test/interop/gen_spandsp_capture.c $(SPANDSP_LIB)
	@mkdir -p $(BUILD)/capture
	$(CC) $(CFLAGS) -no-pie -I$(SPANDSP)/src -o $(BUILD)/gen_capture $< \
	    $(SPANDSP_LIB) -lm
	@./$(BUILD)/gen_capture $(BUILD)/capture

interop: $(BUILD)/test/t_spandsp_b103 $(BUILD)/test/t_spandsp_v23 \
        $(BUILD)/test/t_spandsp_v8 \
        $(BUILD)/test/t_spandsp_v8neg $(BUILD)/test/t_spandsp_v8sock \
        $(BUILD)/test/v8peer $(BUILD)/test/v8peer_ref
	@./$(BUILD)/test/t_spandsp_b103
	@./$(BUILD)/test/t_spandsp_v23
	@./$(BUILD)/test/t_spandsp_v8
	@./$(BUILD)/test/t_spandsp_v8neg
	@./$(BUILD)/test/t_spandsp_v8sock

# V.23, four directions: two channels each way.  Same 64-bit build as the
# Bell 103 interop test and for the same reason.
$(BUILD)/test/t_spandsp_v23: test/interop/t_spandsp_v23.c \
        test/interop/runtime64.c $(SRC) $(CXXOBJ64) | $(BUILD)
	@test -f $(SPANDSP_LIB) || { \
	    echo "SpanDSP not built; run: (cd $(SPANDSP) && ./configure && make)"; \
	    exit 1; }
	@mkdir -p $(BUILD)/test
	$(CC) $(CFLAGS) -no-pie -I$(SPANDSP)/src -o $@ test/interop/t_spandsp_v23.c \
	    test/interop/runtime64.c $(SRC) $(CXXOBJ64) $(SPANDSP_LIB) -lm

V8NEG_SRC  := test/interop/v8neg.c test/interop/v8spandsp.c \
              test/interop/runtime64.c

$(BUILD)/test/t_spandsp_v8neg: test/interop/t_spandsp_v8neg.c \
        $(V8NEG_SRC) $(SRC) $(CXXOBJ64) | $(BUILD)
	@test -f $(SPANDSP_LIB) || { \
	    echo "SpanDSP not built; run: (cd $(SPANDSP) && ./configure && make)"; \
	    exit 1; }
	@mkdir -p $(BUILD)/test
	$(CC) $(CFLAGS) -no-pie -I$(SPANDSP)/src -Itest/interop -o $@ \
	    test/interop/t_spandsp_v8neg.c $(V8NEG_SRC) $(SRC) $(CXXOBJ64) \
	    $(SPANDSP_LIB) -lm

$(BUILD)/test/t_spandsp_v8sock: test/interop/t_spandsp_v8sock.c \
        $(V8NEG_SRC) $(SRC) $(CXXOBJ64) | $(BUILD)
	@test -f $(SPANDSP_LIB) || { \
	    echo "SpanDSP not built; run: (cd $(SPANDSP) && ./configure && make)"; \
	    exit 1; }
	@mkdir -p $(BUILD)/test
	$(CC) $(CFLAGS) -no-pie -I$(SPANDSP)/src -Itest/interop -o $@ \
	    test/interop/t_spandsp_v8sock.c $(V8NEG_SRC) $(SRC) $(CXXOBJ64) \
	    $(SPANDSP_LIB) -lm

# The peer, twice.  64-bit against the reconstruction...
$(BUILD)/test/v8peer: test/interop/v8peer.c test/interop/v8neg.c \
        test/interop/runtime64.c $(SRC) $(CXXOBJ64) | $(BUILD)
	@mkdir -p $(BUILD)/test
	$(CC) $(CFLAGS) -no-pie -Itest/interop -o $@ test/interop/v8peer.c \
	    test/interop/v8neg.c test/interop/runtime64.c $(SRC) \
	    $(CXXOBJ64) -lm

# ...and 32-bit against the blob, which is the only way SpanDSP can be made
# to talk to the original: it is i386 and the SpanDSP here is amd64, so they
# cannot share a process, only a socket.
$(BUILD)/test/v8peer_ref: test/interop/v8peer.c test/interop/v8neg.c \
        $(OBJ) $(HARNESS_OBJ) $(REF) | $(BUILD)
	@mkdir -p $(BUILD)/test
	$(CC) $(ARCH32) $(FPFLAGS) $(CFLAGS) $(REPRODUCE) -DV8PEER_REF \
	    -Itest/interop -Itest/harness $(LDFLAGS) -o $@ \
	    test/interop/v8peer.c test/interop/v8neg.c \
	    $(OBJ) $(HARNESS_OBJ) $(REF) -lm

$(BUILD)/test/t_spandsp_v8: test/interop/t_spandsp_v8.c test/interop/runtime64.c $(SRC) $(CXXOBJ64) | $(BUILD)
	@test -f $(SPANDSP_LIB) || { \
	    echo "SpanDSP not built; run: (cd $(SPANDSP) && ./configure && make)"; \
	    exit 1; }
	@mkdir -p $(BUILD)/test
	$(CC) $(CFLAGS) -no-pie -I$(SPANDSP)/src -o $@ test/interop/t_spandsp_v8.c \
	    test/interop/runtime64.c $(SRC) $(CXXOBJ64) $(SPANDSP_LIB) -lm

$(BUILD)/test/t_spandsp_b103: $(INTEROP_SRC) $(SRC) $(CXXOBJ64) | $(BUILD)
	@test -f $(SPANDSP_LIB) || { \
	  echo "$(SPANDSP_LIB) not built -- see third_party/README.md"; \
	  echo "(do NOT substitute the distro libspandsp: 0.0.6 has the"; \
	  echo " Bell 103 channels swapped)"; exit 1; }
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -no-pie -I$(SPANDSP)/src -o $@ $(INTEROP_SRC) $(SRC) \
	    $(CXXOBJ64) $(SPANDSP_LIB) -lm

# The reconstruction must not depend on 32-bit; only the reference does.
check64:
	@$(CC) $(SYNCFLAGS) -fsyntax-only $(SRC) \
	  && $(CXX) $(SYNCXXFLAGS) -fsyntax-only $(CXXSRC) \
	  && $(CC) $(SYNCFLAGS) $(REPRODUCE) -fsyntax-only $(SRC) \
	  && $(CXX) $(SYNCXXFLAGS) $(REPRODUCE) -fsyntax-only $(CXXSRC) \
	  && echo "64-bit clean, both configurations: OK"

# Regenerate the analysis documents from the blob.
# Two numbers, both from symbol tables rather than from grepping source:
# how much of the blob has a same-named function in this tree, and how much
# of that some test drives against the blob itself.
# $(REF) is a prerequisite because coverage.py reads the alias set out of it:
# which symbols have a `ref_` name is what decides the `tested` denominator,
# and asking that question of an object that is not there gets an answer that
# is plausible and wrong.
# The period-toolchain build and the similarity ratchet.  NOT part of `phase`:
# it needs docker and the tools/toolchain image, which not every checkout will
# have, and it answers a different question from correctness -- see finding 349.
.PHONY: similarity
similarity:
	tools/toolchain/build.sh
	python3 tools/toolchain/compare.py --ratchet

coverage: $(BUILD)/tumap.json $(OBJ) $(REF)
	@$(PYTHON) tools/coverage.py --md docs/coverage.md

$(BUILD)/tumap.json: tools/tumap.py $(BLOB) | $(BUILD)
	@$(PYTHON) tools/tumap.py $(BLOB) --json $@ >/dev/null

docs:
	$(PYTHON) tools/tumap.py    $(BLOB) --md docs/modules.md
	$(PYTHON) tools/tuattrib.py $(BLOB) --verify --md docs/attribution.md \
		--json docs/attribution.json

clean:
	rm -rf $(BUILD) build-cov
	@rm -f a-*.d *.d

# Auto-generated header dependencies (-MMD), so editing a header rebuilds
# everything that includes it.
-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
