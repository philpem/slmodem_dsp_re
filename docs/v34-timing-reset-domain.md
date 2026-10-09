# V34 timing reset: typed history and reset groups

Baseline4bb720f3 after issue260 merged and PR281 rebased onto16cb3384.
Gentoo production300/300 passes, whole baseline1075/1852 exact, ratchetgreen.

Original rxtiminginit0x5b9c0 resets the timing IIR second history taps with
separate short stores at receiver+0x20e and+0x20c. Source instead initializes
`dp.point`, the alternate packed constellation decision view, with one int
store. Existing v34recv.h dp.iir2.q/i provides the actual timing view; no new
union, offset casts, field widths or structure maps are needed. The original
also groups timing/PPM/carrier/history resets in a different order from source.

Four full V34RX TU cells cross baseline/original store order and packed-word/
separate typed IIR halves. Do not vary declarations, registers, flags, pointer
capture or call location. In order-only, keep the packed store at the original
I(-2) slot (no fabricated independent high-half write). The existing helper
call and rx_samples assignment remain first. Both values must become zero.

Prediction: separate IIR-half stores restore the witnessed access widths;
original reset-group order recovers the non-consecutive fields' instruction
order. Falsifier: complete body differs despite both, in which case close this
finite domain rather than permuting equal-valued stores. Raw complete-TU
baseline must reproduce. Audit every function, exports/bindings, nontext
allocated data/BSS/relocations and gain/loss set before adoption. Period gate
is required for any src change. No mutation/fuzzing execution.
