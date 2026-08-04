###########################################################################
#
#	dsplibs -- reconstruction of slmodemd/dsplibs.o
#
#	Self-contained: this tree never builds from, or writes into,
#	../slmodemd or ../re.  The only thing it reads outside itself is
#	the reference object ../slmodemd/dsplibs.o.
#
###########################################################################

BLOB       := ../slmodemd/dsplibs.o
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
CFLAGS     := -Wall -Wextra -Wno-unused-parameter -g -O2 -Iinclude $(DEPFLAGS)

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

SRC        := src/core/encode.c src/service/pcm.c src/core/fixedrc.c src/core/rc_coeffs.c src/core/dp_param.c src/core/dp_wrapper.c src/pump/b103/b103.c src/dsp/fpm_sqrt.c src/dsp/fpm_phasor.c src/dsp/fpm_tone.c src/dsp/fpm_rms.c src/dsp/fpm_div.c src/dsp/fpm_mrf.c src/dsp/fpm_fsm.c src/dsp/fpm_tone_cfg.c src/dsp/fpm_fsd.c src/dsp/fpm_mtd.c src/dsp/fpm_mtd_cfg.c src/dsp/fpm_iir.c src/dsp/fpm_iir_coeffs.c src/dsp/fp_math.c src/dsp/fpm_agc.c src/pump/b103/b103_agc_cfg.c src/pump/b103/b103fp.c src/pump/b103/b103_cfg.c src/pump/b103/b103_tables.c src/callprog/callprog_status.c src/callprog/callprog_cfg.c src/callprog/callprog.c src/callprog/cpfiltrs.c src/callprog/toneiir.c src/callprog/dualtone.c src/callprog/callingtone.c src/callprog/cadence.c src/callprog/elliptic.c src/dialer/dialercfg.c src/dialer/dialer.c src/call/pulse.c src/call/call.c src/v8/v8util.c src/v8/v8v21.c src/v8/v8seq.c src/v8/v8hs.c src/v8/v8sig.c src/v8/v8agc.c src/v8/v8jm.c src/v8/v8dp.c src/v8/v8hsrx.c src/v8/v8handshak.c src/v8/v8proc.c src/pump/v23/v23filt.c src/pump/v23/v23tx.c src/pump/v23/v23rx.c src/pump/v23/bwchdem.c src/pump/v23/v23modem.c src/pump/v23/v23.c src/pump/v34/detector.c src/pump/v34/dftc.c src/pump/v34/dpsk.c src/pump/v34/v34filters.c src/pump/v34/v34rx.c src/pump/v34/v34shell.c src/pump/v34/v34pcmif.c src/pump/v34/v34hshak.c src/pump/v34/v34info.c src/pump/v34/v34scram.c src/pump/v34/v34digital.c
CXXSRC     := src/dsp/FloatIIR.cpp src/pump/v34/v34pcmmain.cpp
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

HARNESS    := test/harness/harness.c test/harness/runtime.c \
              test/harness/fakedp.c
HARNESS_OBJ:= $(patsubst %.c,$(BUILD)/%.o,$(HARNESS))

TESTS      := t_encode t_pcm t_fixedrc t_fpm_sqrt t_rcresample t_dp_param t_dp_wrapper t_b103_reg t_fpm_phasor t_fpm_tone t_fpm_rms t_fpm_div t_fpm_mrf t_fpm_mrf_filter t_fpm_fsm t_fpm_fsd t_fpm_mtd t_fpm_iir t_fp_math t_fpm_agc t_b103fp t_b103hdx t_b103link t_b103alloc t_b103create t_b103dp t_b103direct t_toneiir t_callprog t_callprog_create t_dualtone t_callingtone t_cadence t_dialercfg t_dialer t_dialerprog t_dialstring t_callprog_progress t_call t_calldirect t_v8util t_v8sig t_v8jm t_v8dp t_v8direct t_v8hs t_pulse t_v23filt t_v23tx t_v23rx t_v23bwch t_v23modem t_v23dp t_v23direct t_v34det t_v34dft t_v34fsk t_v34ec t_v34eq t_v34rx t_v34demod t_v34shell t_v34pcmif t_v34hshak t_v34info t_v34scram t_v34digital t_spandsp_replay
CXXTESTS   := t_genericiir t_v34mp
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

# --- compilation ----------------------------------------------------------

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(ARCH32) $(FPFLAGS) $(CFLAGS) $(REPRODUCE) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(ARCH32) $(FPFLAGS) $(CXXFLAGS) $(REPRODUCE) -c $< -o $@

# Linked with $(CC), not $(CXX): the C++ here is -fno-exceptions -fno-rtti with
# no virtuals and no new/delete, exactly as the original was built, so nothing
# needs libstdc++ -- which is just as well, since the 32-bit one is often not
# installed alongside a 64-bit toolchain.
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

test: firewall strings offsets refs $(TESTBIN)
	@rc=0; for t in $(TESTBIN); do ./$$t || rc=1; done; exit $$rc

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

# Everything a phase boundary is supposed to check, in one target.
#
# This exists because `make test` and `make interop` link DIFFERENT runtimes,
# so a module added to $(SRC) can build and pass every differential test while
# leaving the interop tier unbuildable.  That is exactly what happened when
# v34filters.c became the first module to import the debug hooks: 546 tests
# passed and `make interop` had been broken for two commits.
#
# Run this at every phase boundary, not `make test`.
phase: test check64 interop coverage debugcov
	@echo
	@echo "phase boundary: differential, 64-bit, interop, coverage and debug sites all OK"

# SpanDSP interop.  A SEPARATE 64-bit binary: the system SpanDSP is amd64 and
# the blob is i386, so the two tiers cannot share a build.  That is a feature --
# this tier answers "is it a correct Bell 103 modem", which the blob cannot be
# the judge of.  Needs libspandsp-dev.
INTEROP_SRC := test/interop/t_spandsp_b103.c test/interop/runtime64.c
SPANDSP     := third_party/spandsp
SPANDSP_LIB := $(SPANDSP)/src/.libs/libspandsp.a

# Freeze a SpanDSP signal so the 32-bit differential harness can replay it.
capture: $(BUILD)/capture/spandsp_b103.pcm

$(BUILD)/capture/spandsp_b103.pcm: test/interop/gen_spandsp_capture.c $(SPANDSP_LIB)
	@mkdir -p $(BUILD)/capture
	$(CC) $(CFLAGS) -I$(SPANDSP)/src -o $(BUILD)/gen_capture $< \
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
        test/interop/runtime64.c $(SRC) | $(BUILD)
	@test -f $(SPANDSP_LIB) || { \
	    echo "SpanDSP not built; run: (cd $(SPANDSP) && ./configure && make)"; \
	    exit 1; }
	@mkdir -p $(BUILD)/test
	$(CC) $(CFLAGS) -I$(SPANDSP)/src -o $@ test/interop/t_spandsp_v23.c \
	    test/interop/runtime64.c $(SRC) $(SPANDSP_LIB) -lm

V8NEG_SRC  := test/interop/v8neg.c test/interop/v8spandsp.c \
              test/interop/runtime64.c

$(BUILD)/test/t_spandsp_v8neg: test/interop/t_spandsp_v8neg.c \
        $(V8NEG_SRC) $(SRC) | $(BUILD)
	@test -f $(SPANDSP_LIB) || { \
	    echo "SpanDSP not built; run: (cd $(SPANDSP) && ./configure && make)"; \
	    exit 1; }
	@mkdir -p $(BUILD)/test
	$(CC) $(CFLAGS) -I$(SPANDSP)/src -Itest/interop -o $@ \
	    test/interop/t_spandsp_v8neg.c $(V8NEG_SRC) $(SRC) \
	    $(SPANDSP_LIB) -lm

$(BUILD)/test/t_spandsp_v8sock: test/interop/t_spandsp_v8sock.c \
        $(V8NEG_SRC) $(SRC) | $(BUILD)
	@test -f $(SPANDSP_LIB) || { \
	    echo "SpanDSP not built; run: (cd $(SPANDSP) && ./configure && make)"; \
	    exit 1; }
	@mkdir -p $(BUILD)/test
	$(CC) $(CFLAGS) -I$(SPANDSP)/src -Itest/interop -o $@ \
	    test/interop/t_spandsp_v8sock.c $(V8NEG_SRC) $(SRC) \
	    $(SPANDSP_LIB) -lm

# The peer, twice.  64-bit against the reconstruction...
$(BUILD)/test/v8peer: test/interop/v8peer.c test/interop/v8neg.c \
        test/interop/runtime64.c $(SRC) | $(BUILD)
	@mkdir -p $(BUILD)/test
	$(CC) $(CFLAGS) -Itest/interop -o $@ test/interop/v8peer.c \
	    test/interop/v8neg.c test/interop/runtime64.c $(SRC) -lm

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

$(BUILD)/test/t_spandsp_v8: test/interop/t_spandsp_v8.c test/interop/runtime64.c $(SRC) | $(BUILD)
	@test -f $(SPANDSP_LIB) || { \
	    echo "SpanDSP not built; run: (cd $(SPANDSP) && ./configure && make)"; \
	    exit 1; }
	@mkdir -p $(BUILD)/test
	$(CC) $(CFLAGS) -I$(SPANDSP)/src -o $@ test/interop/t_spandsp_v8.c \
	    test/interop/runtime64.c $(SRC) $(SPANDSP_LIB) -lm

$(BUILD)/test/t_spandsp_b103: $(INTEROP_SRC) $(SRC) | $(BUILD)
	@test -f $(SPANDSP_LIB) || { \
	  echo "$(SPANDSP_LIB) not built -- see third_party/README.md"; \
	  echo "(do NOT substitute the distro libspandsp: 0.0.6 has the"; \
	  echo " Bell 103 channels swapped)"; exit 1; }
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SPANDSP)/src -o $@ $(INTEROP_SRC) $(SRC) \
	    $(SPANDSP_LIB) -lm

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
