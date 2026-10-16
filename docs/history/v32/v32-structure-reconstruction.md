# V.32 structure reconstruction

Byte-fidelity experiments are paused while raw offset accesses are replaced
with evidence-backed shared types. PRs #97 and #98 were merged before this
work. This is a structural reconstruction task, not a modern-compiler repair.

## First tranche

The sequence codec, generator and detector use two regions whose field widths
and roles are already documented in `include/dsplib/v32seq.h`. Model those
regions in one shared header and migrate `src/pump/v32/v32seq.c`, keeping the
mutation substitutions synchronized with the new field expressions.

The DSP rate indices at +0x28 and +0x2a are signed shorts. The handshake
register accessor exposes five shorts at +0x3c. Construction clears seven
shorts, but that alone does not establish a seven-element register bank:
the words at +0x46 and +0x48 must remain neutrally named.

Generator state occupies unsigned shorts at +0x4a through +0x52. Detector
width is an unsigned short at +0x54; its masks, target, shift register and
latched match occupy ints at +0x58 through +0x68. Keep the alignment gap
explicit. Offset assertions must be active under GCC 3.4.2, without depending
on newer pointer-size predefines.

Remove the redundant casts in the `short V32_ESEQ[]` initializer. Do not
extend that cleanup to computed-value narrowing: the promoted quotient in
`InitGenSequence`, for example, must remain promoted until its field stores.

## Owner layouts still to reconstruct

The allocation sites in `src/pump/v32/V32.c` establish a 0x6c root, a 0xb0
handshake allocation and a 0x50dc DSP allocation. `struct v32_dp` is a
different, outer datapump wrapper; it must not be substituted for this root.
The root begins with the existing `struct v32fp_params` and has handshake
and DSP pointers at +0x64 and +0x68.

Existing nested types have one home already: reuse the scrambler, symbol
mapper, pulse shaper, multirate filter, echo canceller, timing recovery and
equalizer definitions rather than making offset-equivalent duplicates.

The first sequence views are deliberately incomplete. They do not settle
pointer ownership, constructor/destructor coverage or the full allocation
layout. Native pointer slots in the original ABI are four bytes; a wider
host representation must not dictate the reconstructed period layout.

Next steps:

1. Recover the complete root and handshake owners from allocation,
   initialization, dispatch and destruction, resolving ambiguous counters
   from their actual uses.
2. Compose the DSP owner from the existing embedded types and establish the
   boundaries and ownership of its heap buffers.
3. Migrate the remaining V.32 consumers and retire obsolete raw-access
   macros and comments as their last consumers disappear.
4. Migrate V.22 accesses into its existing `v22fp` model before inventing new
   types; older headers still describe construction as unreconstructed.
5. Continue through the fax raw-access users, with the same one-type-one-home
   and evidence requirements.

## Validation requirement

The first tranche is not validated merely by adding layout assertions or
updating mutation anchors. Before committing, run the Gentoo GCC 3.4.2-r2
`make phase` gate and record its differential denominator and structural
results. Byte comparisons can reveal unintended changes, but matching bytes
are not a substitute for reconstructing missing owner types.

## Completed owner migration and endian boundary

The owner migration described above is now implemented for V.32, V.22,
V.21, V.17, V.27 and V.29. Shared decoder/Viterbi objects are embedded as
real types rather than byte arrays. Unknown meanings retain neutral names;
external report prefixes describe only their observed extent. The V32_ESEQ
initializer no longer repeats redundant short casts.

The Gentoo GCC 3.4.2-r2 reconstruction gate passed 375 tests, with zero
failures, after the final embedded-Viterbi fixture correction. Artifact:
`build/structure-embedded-viterbi-complete/gates.log`. Structural checks
also passed, including all 10041 anchors in 272 mutation suites. This is
functional and structural evidence, not a byte-fidelity improvement claim.

### Byte/word overlays are not an endian-portability claim

The recovered status/result unions model physical i386 byte offsets combined
with native word loads, masks and stores. Their named byte fields are not
portable numeric low/high-byte accessors. On a big-endian target the same
physical byte and numeric word no longer denote the same bits. V.32's returned
status word is one affected consumer; V.17/V.21/V.27/V.29 combine whole-word
operations with partial byte updates too.

`period_byte_layout.h` therefore rejects unsupported byte order explicitly.
It does not implement big-endian support. Reversing the union members alone
is not a justified repair: raw owner consumers and external report prefixes
also depend on physical offsets. A future port must classify internal numeric
flags separately from caller-visible byte layouts, audit aliases and partial
write ordering, and add independent big-endian tests. Period x86 differential
tests cannot establish that portability property.

After adding the explicit endian boundary, the full Gentoo gate passed again:
375 passed, 0 failed; period differential and structural checks all OK.
Artifact: `build/structure-endian-boundary-authorized/gates.log`.
The preceding `structure-endian-boundary` attempt was invalid for differential
validation because sandbox permissions denied the Docker socket; no compiler
or source failure was inferred from it. No big-endian execution was performed.
