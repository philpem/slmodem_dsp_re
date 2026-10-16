# Cleaned-sample getter: original accepted-count path

The original `V32FP_GetCleanedSamples` at 0x7f910 compares the sixteen-bit
count against 160 and branches on unsigned greater-than. The accepted-count
store falls through. Retained source places the zero-count arm first and
produces the opposite branch. Both reload the cleaned-buffer pointer after
the output store, but only the original also reloads the modem's owner.

Test exactly two complete-TU cells: unchanged source and accepted-count arm
first, preserving every type, expression, store and read. Prediction: source
arm polarity restores the original branch layout. Falsifier: the branch stays
inverted or other instructions still prevent complete identity. This does
not authorize alias casts, volatile reads or forced reloads to reproduce the
second owner access. Such a residual needs independent original alias/type
or common-profile evidence.

Use the current Gentoo baseline configuration and bug reproduction define;
require a raw unchanged-object repeat before interpreting the control. Audit
all shared function bodies and allocated data/metadata. Adopt only strict
identity with no unexplained bystander changes.
