# ADID alternate threshold initialization lifetimes

Pinned856c1ecb. Original getAltVarThresh initializes sum at0x40651, performs its first six-element sum loop, then initializes count at0x4067b and below at0x4067d. Baseline clears count at+0x40a and below at+0x414 before that loop. These are separate live ranges across actual arithmetic, not arbitrary declarations or register allocation requests. Prior domains concern NaN/threshold math and diagnostics, not this observed initialization boundary.

Predeclare four full-TU cells: baseline; delay below initialization until after lim assignment; delay count initialization there; both. Keep declaration sites, all types, float conversion/arithmetic/grouping, signed comparisons, diagnostics and return thresholds unchanged. Original source receives valid member sizes; no new assumptions introduced. Strict target exactness and all-body/data/metadata/canonical reloc audit mandatory; no bystander-only adoption.

Diagnostic-only correction: full -da baseline reproduces the previously recorded ADID compiler ICE (batch50 verdict domain). That failed run is retained in gcc3-batch100-v90-adid-alt-init, explicitly invalid. Replay uses -dr initial RTL exactly as the earlier ADID authority; unchanged raw bytes must reproduce before accepting any source result. Production compiler profile is unchanged.

Measured: Four valid -dr cells raw reproduce baseline, all retain getAltVarThresh SIZE10 with no gains/losses. The late initializer source does not change emitted body, so this family is closed.
