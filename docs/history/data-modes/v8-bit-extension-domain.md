# V8 bit-input extension: independent declared transfer

Base38626248. Original four bit-push sites read mark/space words with MOVZWL
(+4/+6 relative to original parameter owner), while retained helper arguments
are short and read those same named fields with MOVSWL (+10/+12 relative to
our larger parameter owner), then cast bit to ushort in the body. Parameter
owner displacement is separately unresolved; no fabricated subobject here.

Five cells: unchanged raw baseline; published-zero signed-bit prior control;
published-countdown signed-bit prior control; both corresponding unsigned-short
bit formal controls. Change formal bit in push_bits/drain_run/flush_run only;
header field types and actual 16bit values are unchanged. Bit accumulator body
already casts bit to unsigned short, so passing through the unsigned formal
preserves every bit pattern. No register/declaration/profile permutations.
Predict source formal conversion restores zero-extension at caller capture;
trace the earliest RTL transition and final bit-input loads. This is distinct
from the completed remainder/countdown domain. Complete all five cells then
stop/reframe; audit every emitted body/metadata/data/relocation, not just size.
No source adoption unless complete exact gain plus period gate. No runtime,
fuzzing/mutation/#22 or other-session #263 files.
