# Diffcoder operand-order recovery

Domain posted before compilation as [the diffcoder operand-order comment in
#22](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5964857877).
Four labels (baseline control, decoder-flip, encoder-flip, both-flip) over the
two defining TUs, `src/pump/v90/V90SignBitsExtractor.cpp` and
`src/pump/v90/V90SpectralShaper.cpp`, driven by
`tools/playbook_diffcoder_flip.py` over `tools/playbook_small_patterns.py` at
`47174bf3157a98e54d5bb5d9a62b6e77bb8bc19d`, header overlay
`include/dsplib/DiffCoder.h` only. Gentoo GCC 3.4.2-r2
(`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`,
`/usr/i386-pc-linux-gnu/gcc-bin/3.4`), executed assembler GNU as 2.15.92.0.2
20040927, saved complete C++ profile with `DSPLIB_REPRODUCE_BUGS` appended
last. Artifacts: `build/playbook-diffcoder-flip/results.json`.

## The two residuals and the hypothesis

Both `ParallelDifferential<unsigned char>::process` residuals are operand-ROLE
swaps. The encoder (54/54 bytes, BYTES 4, row 11 USE CONFLICT) had blob
`movzbl (%edx),%eax` (acc <- *state) then `xor (%ebx),%al` (mem-xor *in)
against our acc <- *in / mem-xor *state. The decoder (58 vs 57 bytes, SIZE 1,
row 11 NON-REGISTER OPERAND) had blob `mov %dl,%al; xor (%ecx),%al` against
our `movzbl (%ecx),%eax; xor %dl,%al`. Cross-function pattern: our compiler
always fed the accumulator from the tree's second operand, the blob from the
first. Same compiler, different trees, so the hypothesis was that the author's
source operand order is flipped in both bodies. This was the new discriminator
`docs/parallel-decoder-retained-result.md` required before any further
spelling search.

## Results

| label | SignBitsExtractor TU | SpectralShaper TU |
| --- | --- | --- |
| baseline | 8/12 exact, raw-reproduces | 10/15 exact, raw-reproduces |
| decoder-flip | byte-identical to baseline | byte-identical to baseline |
| encoder-flip | byte-identical to baseline | 11/15, encoder EXACT |
| both-flip | byte-identical to baseline | 11/15, encoder EXACT |

Gains: `_ZN27ParallelDifferentialEncoderIhE7processEPhS1_` EXACT at 54 bytes,
both relocation-free. Losses: none, in either TU, in any cell. Changed bodies
per cell: only the encoder process; the encoder candidate body is
instruction-for-instruction the blob's, including `inc %ebx` moved after the
`xor (%ebx),%al` as the memory-operand role requires.

## The decoder negative, and what it decodes

The decoder flip emitted a byte-identical object: the source order of a
REG+MEM commutative pair is canonicalized away (REG-first), so no spelling of
`x ^ *state` reaches the blob's acc<-REG-copy + mem-xor form. The re-read
spelling `*out = *in ^ *state; *state = *in;` was excluded before compiling:
it re-reads `*in` after the `*out` store and changes behavior when
`out == in`, while the recorded 40,421,984-check parity (t_diffcoder, fuzz
groups in findings) pins the blob to the one-read temp form. The decoder
residual is therefore the reload two-address form choice for a REG+MEM
commutative op -- an allocation-stage question, not a source-order one, and
not attributable to any spelling of the operands.

For the two-MEM encoder the source order does survive to RTL and decides the
roles; that asymmetry is itself the measured fact.

## Adoption

`include/dsplib/DiffCoder.h` adopts only the encoder operand order, with a
comment recording the observed roles. The flip is behavior-identical
(commutative XOR), so no differential fixture changes; t_diffcoder's 13.6M
encoder/decoder checks and the Scrambler/Descrambler controls are unaffected.
No fuzzing or mutation execution; static anchor checks only. Full-tree
census, ratchet, fixed period phase and the 300-object partial review are the
adoption gates; see the finding and the PR ledger for their measured results.
No uniqueness claim: one exact match inside a declared four-label family
decodes the operand-order fact, not the original spelling.
