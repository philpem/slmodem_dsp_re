# FloatFIR: consume the entry count before member acquisition

One candidate plus raw baseline, ca84f1cc, complete FloatFIR.cpp TU with unchanged
Gentoo production flags and bug reproduction. No source/register permutations.

New discriminator: installed scratch tracing locates the reset register-only
residual in gen_peephole2_1229's split of bufferLength-minus-taps. The preceding
block processor makes exactly one such search. Original block processing at
0x46b1f and0x46bdb decrements count and compares UINT_MAX at BOTH entry and tail.
The previously closed while-postdecrement experiment retained an independent
zero guard and added a second entry test after member loads. This new cell
consumes the entry count in the existing pre-load guard, then postdecrements
at the tail. It preserves the authentic zero boundary: no member or buffer
access at count zero. It does not remove the guard or move owner loads before it.

Prediction: one DEC/CMP UINT_MAX entry and one tail sentinel, without the old
control's extra test. The loop runs exactly the same unsigned count of iterations,
including count one and UINT_MAX. Hypothesis fails if that structure does not
appear. Independently observe any later scratch changes; a bystander gain alone
cannot justify an unsupported processor body. Audit all 8 body verdicts,
bindings, nontext data/relocations, stage streams and raw baseline identity.
No adoption based on smaller size. No fuzzing/mutation execution.
