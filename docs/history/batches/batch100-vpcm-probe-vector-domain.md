# VPcm probe transformation vector

Pinned856c1ecb. Original getUinfoValue has a25-float stack vector at stack+0x30..0x90. At0xf05d it stores the float-narrowed scaled probe; at0xf07b it stores the transformed level before checking use[i]. The current source has only two scalar float narrowing boundaries and calculates the final level inside the accepted-tone branch. This is a genuine work-array and computation-placement witness, not frame-size padding.

Predeclare three cells: unchanged full-TU baseline; unconditional scalar level before use[i]; real25-float vector used for scaled and final values before selection. Preserve both existing float scaled/decade conversions, long-double literal precision, tone order, use mask, all L2/member clears/calls/diagnostics. No volatile, spills, artificial padding, headers or flags. The vector's elements are real transformed probe values consumed by the selection operation. Require target strict exactness and complete metadata/data/relocation/all-body audit. A nonexact arithmetic-placement candidate is not adopted on score alone.

Measured: Three valid cells: baseline SIZE152, unconditional scalar SIZE146, consumed vector SIZE20; no gains/losses. Structural original scaled vector write is recovered, but final integer readback differs. No source adoption.
