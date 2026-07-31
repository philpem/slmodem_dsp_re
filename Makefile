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
CFLAGS     := -Wall -Wextra -Wno-unused-parameter -g -O2 -Iinclude -MMD -MP

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

SRC        := src/service/pcm.c src/core/fixedrc.c src/core/rc_coeffs.c src/core/dp_param.c src/core/dp_wrapper.c src/pump/b103/b103.c src/dsp/fpm_sqrt.c src/dsp/fpm_phasor.c src/dsp/fpm_tone.c src/dsp/fpm_rms.c src/dsp/fpm_div.c src/dsp/fpm_mrf.c src/dsp/fpm_fsm.c src/dsp/fpm_tone_cfg.c src/dsp/fpm_fsd.c src/dsp/fpm_mtd.c src/dsp/fpm_mtd_cfg.c src/dsp/fpm_iir.c src/dsp/fpm_iir_coeffs.c src/dsp/fp_math.c src/dsp/fpm_agc.c src/pump/b103/b103_agc_cfg.c src/pump/b103/b103fp.c src/pump/b103/b103_cfg.c src/pump/b103/b103_tables.c src/callprog/callprog_status.c src/callprog/callprog_cfg.c src/callprog/callprog.c src/callprog/cpfiltrs.c src/callprog/toneiir.c src/callprog/dualtone.c src/callprog/callingtone.c src/callprog/cadence.c src/callprog/elliptic.c src/dialer/dialercfg.c src/dialer/dialer.c src/call/pulse.c src/call/call.c src/v8/v8util.c src/v8/v8v21.c src/v8/v8seq.c src/v8/v8hs.c src/v8/v8sig.c src/v8/v8agc.c src/v8/v8jm.c
CXXSRC     := src/dsp/FloatIIR.cpp
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

TESTS      := t_pcm t_fixedrc t_fpm_sqrt t_rcresample t_dp_param t_dp_wrapper t_b103_reg t_fpm_phasor t_fpm_tone t_fpm_rms t_fpm_div t_fpm_mrf t_fpm_mrf_filter t_fpm_fsm t_fpm_fsd t_fpm_mtd t_fpm_iir t_fp_math t_fpm_agc t_b103fp t_b103hdx t_b103link t_b103alloc t_b103create t_b103dp t_toneiir t_callprog t_callprog_create t_dualtone t_callingtone t_cadence t_dialercfg t_dialer t_dialerprog t_callprog_progress t_call t_v8util t_v8sig t_v8jm t_pulse t_spandsp_replay
CXXTESTS   := t_genericiir
TESTBIN    := $(addprefix $(BUILD)/test/,$(TESTS) $(CXXTESTS))

REF        := $(BUILD)/dsplibs_ref.o
SYMMAP     := $(BUILD)/symmap.txt

# dsplibs.o predates modern hardening defaults: it wants an executable stack
# and has relocations in .rodata, which a PIE link cannot satisfy.  Both are
# properties of the 2003 reference object, not of anything we build, so relax
# them for test binaries only.
LDFLAGS    := -no-pie -Wl,-z,noexecstack,-z,notext

.PHONY: all test check64 docs clean interop capture

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
	$(PYTHON) tools/symmap.py $(BLOB) -o $@

$(REF): $(SYMMAP) $(BLOB)
	objcopy --redefine-syms=$(SYMMAP) $(BLOB) $@

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

test: $(TESTBIN)
	@rc=0; for t in $(TESTBIN); do ./$$t || rc=1; done; exit $$rc

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

interop: $(BUILD)/test/t_spandsp_b103
	@./$(BUILD)/test/t_spandsp_b103

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
	@$(CC) $(CFLAGS) -fsyntax-only $(SRC) \
	  && $(CXX) $(CXXFLAGS) -fsyntax-only $(CXXSRC) \
	  && $(CC) $(CFLAGS) $(REPRODUCE) -fsyntax-only $(SRC) \
	  && $(CXX) $(CXXFLAGS) $(REPRODUCE) -fsyntax-only $(CXXSRC) \
	  && echo "64-bit clean, both configurations: OK"

# Regenerate the analysis documents from the blob.
docs:
	$(PYTHON) tools/tumap.py    $(BLOB) --md docs/modules.md
	$(PYTHON) tools/tuattrib.py $(BLOB) --verify --md docs/attribution.md \
		--json docs/attribution.json

clean:
	rm -rf $(BUILD)

# Auto-generated header dependencies (-MMD), so editing a header rebuilds
# everything that includes it.
-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
