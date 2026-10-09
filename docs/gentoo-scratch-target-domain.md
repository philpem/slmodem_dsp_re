# Current register-only targets: scratch-search boundary

Baseline ca84f1cc, fixed production Gentoo flags/configuration and bug reproduction.
Four unchanged complete translation units: toneiir.c, FloatFIR.cpp, v22.c and
V27_SDM.c. Targets: _iir_filter_create, FloatFIR::reset, dp_v22_init and
SDMv27_init. These targets are alpha-equivalent to the reference, not strict
byte-exact. Each complete object must raw-reproduce production before tracing.
No source alternatives, forced cursor/registers, profile changes or runtime
fuzz/mutation tests belong to this domain. PR263's files are excluded.

Prediction: the actual trace distinguishes targets requesting scratch registers
from targets making no searches. A zero-search target refutes direct persistent
scratch selection as the explanation for its own colors; it does not exclude
other TU state or later renaming. Nonzero searches alone do not prove the cause
of a reference mismatch. Match input patterns, generator, cursor, availability
and result must be retained; no instruction-order mapping to the original is
assumed. Preserve all complete objects and body verdicts.

Four baseline compilations, then five observed/raw replays: each baseline once
and an independent V27_SDM repeat. Repeat complete events must agree. Existing
two positive scratch controls and four malformed-trace controls remain the
validation authority for the model, which is limited to HI/SI single GPRs.
A result outside that domain stops interpretation. All emitted functions, even
ones requesting zero searches, must appear in the output with an explicit
count. A missing target is a failure, not zero. Follow up only on independently
supported original operand, use, visibility or emission-boundary differences.
