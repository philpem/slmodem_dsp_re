# VPcm probe vector final conversion boundary

Predeclare baseline, explicit-final-cast work vector positive control, and same consumed work vector with ordinary implicit float assignment at its final level store. Prior three-cell vector domain restored scaled FSTS but explicit final (float) forces an extra FSTPS/MOV transfer. Original0xf07b FSTS stores the final float value while retaining x87 value for0xf081 FSTPS L2 store; there is no intervening explicit narrowing scratch store. The original thus supports ordinary array assignment instead of an extra explicit cast at this final store. Keep scaled and decade casts exactly fixed; constants stay long double. No flags, headers, extra memory padding or register requests. Full TU unchanged raw baseline and all-body/data/metadata audit mandatory.

Measured: Three valid cells: explicit and implicit final cast vectors both SIZE20, identical emitted target. No gains/losses. Redundant cast removal inert; no adoption.
