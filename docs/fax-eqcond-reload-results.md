# TxHdxEQCondV27 operand and lifetime controls

Base 9025b8d8. No production source/header changes; no gains or losses.
21 full TUs,210 live canonical body verdicts. Original264 B, baseline224 B.
Rawbaseline SHA256 bb994c010d80cc1c303fa485aface8db67416409d7abd7bb2a062becd4fca692
reproduces in both runs. Nine source-unchanged bystander bodies identical in
every cell; two exact bodies stay exact. Full symbol type/binding/visibility,
allocated noncode bytes, BSS and canonical nontext relocations identical.
Audit rechecks live source/object hashes and every canonical verdict against
saved JSON; it does not trust saved gain lists. No harness/fuzz/mutation runs.

First 16 cells cross only the four witnessed choices declared in
fax-eqcond-reload-domain.md. The all-four source emits243 B; individual sizes
range223..269 B. None is a byte preimage. Second 5 cells repeat raw baseline and
all-four source, then cross pre-fill cursor capture and pre-Scramble count
capture. All four witnessed-source lifetime forms emit243 B, but actual bytes
can differ. No winner chosen by size and no profile/local-slot/order matrix.

The member-subtraction control emits an explicit second countdown read in
initial RTL UID 53. First CSE removes it: initial countdown access UIDs 20,53,56
become 20,56 (one read and one store). Rate reads UIDs 126,135 survive CSE.
Audit parses instruction patterns, excluding dependency notes. Original has
another countdown read at a3bc7; ordinary compound assignment does not retain
it under this compiler/profile. Original second-loop two branch tails remain
duplicated; candidate final assembly shares its output store/decrement tail.
Observed loop shapes and lifetime controls do not explain that difference.
This does not establish any uniquely original compiler option or #22 closure.

Two independent source-fidelity observations remain, even without exact gain:

- Original zero-extends caller budget and compares against positive signed
  countdown. Baseline signed budget can select a negative count when bit 15
  is set; original selects min(positive countdown,unsigned budget). The
  consistent unsignedcount cell restores original operands. No claim that
  all public entry histories exercise high-bit budgets was tested here.
- Original reloads signed rate+0xc separately inside each output branch on
  every iteration. Baseline caches it once after ScrambleDataV27. A current
  output unsigned short can alias corresponding signed short rate storage;
  a store can affect a later rate read. Per-member read cell restores this
  boundary without adding volatile or casts to unrelated types.

Partial controls are diagnostic hypotheses, not an equivalence proof. In
particular, pointer/unsigned-postdecrement controls with the old signedbudget
retain its negative-count issue and may differ further on those states.
No source adopted. These semantic leads need a separately authorized valid
caller/alias lifecycle investigation and period evidence, or a complete byte
preimage, before publication as source corrections. Do not disguise them as
pure register-allocation differences or fix them solely for byte size.

Reproduction from this pinned worktree:

    python3 tools/fax_eqcond_reload_reproduce.py --domain docs/fax-eqcond-reload-domain.md --baseline-dir ../byteexact-x87-scheduling/build/reload-baseline
    python3 tools/fax_eqcond_lifetime_reproduce.py --domain docs/fax-eqcond-reload-domain.md --baseline-dir ../byteexact-x87-scheduling/build/reload-baseline
    python3 tools/fax_eqcond_reload_audit.py

Gentoo GCC 3.4.2-r2, compiler-selected GNU assembler 2.15.92.0.2 20040927,
image ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest. Actual commands saved
for every cell, including retained -O3,-frename-registers,-march=i386,
-mtune=i686,-mfpmath=387,-mno-ieee-fp,-fomit-frame-pointer,
-maccumulate-outgoing-args,period_compat.h,-D__SIZEOF_POINTER__=4,
-v,-save-temps,-da,-dP and DSPLIB_REPRODUCE_BUGS last. Baseline config already
contains the same bug define, so helpers append an identical final define;
this duplication is inert and all raw baseline objects reproduce.
Artifacts:build/fax-eqcond-reloads/results.json,
build/fax-eqcond-lifetimes/results.json,build/fax-eqcond-reload-audit.json.

Bounded family closed absent new independent source/RTL witness. No further
countdown/type/loop/slot/register synonyms inferred from near sizes.
