# Fax binary timing and shared-tail screen

Base fa941457, retained Gentoo GCC 3.4.2-r2 production profile. No production
source/header changes; 0 gains and 0 losses. Read-only screen: 78 fax objects,
317 shared functions, 97 nonexact functions of original size <=350 bytes.
One real duplicate-tail detector positive (FSE_decision_eqtrn original two
returns versus current one) and one instruction-list refusal. Screen retains
all eligible rows, includes EBP as a possible data base, excludes ESP, and
records raw memory/call/branch epochs. It does not equate register bases,
prove alias/dataflow or grade normalized instructions as exact.

Two complete-TU baseline/candidate pairs: four compilations, 20 live canonical
body verdicts. Both raw baselines reproduce retained objects. Only the two
edited targets change; eight bystander body copies remain identical. All
symbol type/binding/visibility/imports, named data, allocated noncode bytes,
BSS and canonical nontext relocations match baseline. Audit rechecks live
source/object hashes and all canonical verdicts against saved JSON.

SDM_descrambler: original167 B, baseline146 B, candidate151 B (SIZE16).
Ordinary `*data++ &= mask` preserves both output stores. It also preserves
its second HI read through global reload, unlike retained source:

| Source | initial RTL | first CSE | combine | global reload | postreload |
| --- | --- | --- | --- | --- | --- |
| retained | 37,47,53,56 | 37,47,56 | 38,56 | — | — |
| postincrement | 37,47,56,59 | 37,47,56,59 | 38,47,56,59 | 38,47,56,59 | 38,47,59 |

UID lists count HI data accesses, excluding RTL notes. In candidate25.greg,
UID56 sets AX:HI from MEM:HI(DX:pointer). At26.postreload it sets AX:HI from
AX:HI instead. The original still rereads through a preserved old cursor and
zero-extends the word at9f276. Two stores and surviving initial CSE therefore
do not establish recovery; distinguish late hard-register value forwarding
from early pseudo-register CSE and store elimination. No near-size adoption.

FSE_decision_eqtrn: original251 B, baseline240 B, branch-local publication251 B
but BYTES199, not exact. Original has two publication/return tails; candidate
still shares its final tail and has a different frame/save/lifetime sequence.
The size equality hides substantial differences (69 original nonpadding
instructions versus63 candidate). Explicit source branch returns alone do
not recover the original tail boundary. Existing tick/abs/owner families were
not retried, nor was the already closed AGC-return family in DemodDataV21.

# Static postreload rule and next discriminator

Read the available official GCC3.4.2 archive; extract only postreload.c and
cselib.c under build/fax-postreload-source. Archive SHA256:
522c53b92ff9096089f3074c50e17a5169952d32f4c883c6fdae350e8f1b344e.
File SHA256 pins: postreload.c a609581a9521dab0a57a962048196700c79bc445ef98cdc80863a83bca99402e;
cselib.c 9a07a46bf20a184c07b2c4e264e248da956ec8f72b4a980b199c65350bfce368.
This is upstream source, not a measured trace of all Gentoo patches.

[postreload.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/postreload.c)
reload_cse_regs_1 processes each instruction with reload_cse_simplify before
cselib_process_insn. reload_cse_simplify_set207..335 accepts a register
destination with a non-register, side-effect-free source. It queries
cselib_lookup242 for the source value, searches its known locations for a
register, and uses validate_change330 when register-copy cost is lower, or
equal with a register replacing a non-register. AX need not be a different
register from the destination: a redundant selfcopy can result, as observed.
The generic operand fallback350 onward can also replace recognized memory
operands with equivalent hard registers; this static reading alone does not
prove which exact fallback/validation branch fired in the installed compiler.

[cselib.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/cselib.c)
lookup_mem741..781 keys memory values by tracked address and mode, refuses
volatile/BLK memory, and returns no known value when the address/mode has none.
record_set1168..1209 associates stores with a source value/address and register
writes with mode-specific register locations. process_insn1330 onward clears
knowledge at labels and volatile asm, invalidates registers/memory on calls,
and records sets. Candidate UID47 writes HI(AX) to its data address; AX remains
unmodified before UID56, and cursor copying/advancing leaves a tracked old
address. That supports, but does not independently instrument, the observed
memory-to-AX forwarding. It is not an arbitrary register-number explanation.

Next bounded discriminator is read-only observation of the installed compiler
at UID56: record whether simplify_set or operand fallback performs validation,
the known source value's mode/address/register locations, and invalidations
between UID47 and56. Compare initial/global RTL to the original instruction's
explicit ZERO_EXTEND:SI(MEM:HI) role. Whole-expression lookup of a widened read
can differ from HI lookup, but operand fallback means width alone is not a
proven barrier. Original RTL is unavailable, so why its read survived remains
unmeasured; address identity, value-mode identity, and available HI locations
are alternatives to discriminate, not conclusions. No new flag control,
volatile/coercive source or register/slot/order permutation is justified here.
These two source controls are closed absent that independent evidence.

# Replay

    python3 tools/fax_binary_timing_screen.py --baseline-dir build/tc_out --census build/byteident-reload-batch.json
    python3 tools/fax_tail_timing_reproduce.py --domain docs/fax-tail-timing-domain.md --baseline-dir build/tc_out
    python3 tools/fax_tail_timing_audit.py

Actual complete compiler commands, hashes and diagnostics saved for every cell;
Gentoo3.4.2-r2 and executed compiler-selected assembler2.15.92.0.2 identified.
Retained flags include -O3 -frename-registers -march=i386 -mtune=i686
-mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args,
period_compat.h and pointer-size define. Helper appends DSPLIB_REPRODUCE_BUGS
last after -v -save-temps -da -dP, retaining the identical baseline bug define.
Artifacts: build/fax-binary-timing-screen.json, build/fax-tail-timing/results.json,
build/fax-tail-timing-audit.json. No candidate runtime/fuzz/mutation runs or source adoption.
