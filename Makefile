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
CFLAGS     := -Wall -Wextra -Wno-unused-parameter -g -O2 -Iinclude
PYTHON     := python3

# x87 with 80-bit intermediates, matching GCC 3.4.2's -m32 default and the
# blob's own code generation (5300 x87 instructions, zero SSE).  Do not
# change this without re-reading docs/findings.md section 8.
FPFLAGS    := -mfpmath=387

SRC        := src/service/pcm.c src/core/fixedrc.c src/core/rc_coeffs.c src/dsp/fpm_sqrt.c
OBJ        := $(patsubst %.c,$(BUILD)/%.o,$(SRC))

HARNESS    := test/harness/harness.c test/harness/runtime.c
HARNESS_OBJ:= $(patsubst %.c,$(BUILD)/%.o,$(HARNESS))

TESTS      := t_pcm t_fixedrc t_fpm_sqrt t_rcresample
TESTBIN    := $(addprefix $(BUILD)/test/,$(TESTS))

REF        := $(BUILD)/dsplibs_ref.o
SYMMAP     := $(BUILD)/symmap.txt

# dsplibs.o predates modern hardening defaults: it wants an executable stack
# and has relocations in .rodata, which a PIE link cannot satisfy.  Both are
# properties of the 2003 reference object, not of anything we build, so relax
# them for test binaries only.
LDFLAGS    := -no-pie -Wl,-z,noexecstack,-z,notext

.PHONY: all test check64 docs clean

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
	$(CC) $(ARCH32) $(FPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/test/%: $(BUILD)/test/unit/%.o $(OBJ) $(HARNESS_OBJ) $(REF)
	@mkdir -p $(dir $@)
	$(CC) $(ARCH32) $(LDFLAGS) -o $@ $^ -lm

$(BUILD)/test/unit/%.o: test/unit/%.c
	@mkdir -p $(dir $@)
	$(CC) $(ARCH32) $(FPFLAGS) $(CFLAGS) -Itest/harness -c $< -o $@

$(BUILD):
	@mkdir -p $(BUILD)

# --- targets --------------------------------------------------------------

test: $(TESTBIN)
	@rc=0; for t in $(TESTBIN); do ./$$t || rc=1; done; exit $$rc

# The reconstruction must not depend on 32-bit; only the reference does.
check64:
	@$(CC) $(CFLAGS) -fsyntax-only $(SRC) && echo "64-bit clean: OK"

# Regenerate the analysis documents from the blob.
docs:
	$(PYTHON) tools/tumap.py    $(BLOB) --md docs/modules.md
	$(PYTHON) tools/tuattrib.py $(BLOB) --verify --md docs/attribution.md \
		--json docs/attribution.json

clean:
	rm -rf $(BUILD)
