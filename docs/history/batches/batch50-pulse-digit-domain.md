# Pulse digit: owner and corrected-count boundaries

At blob 0x3237 the original input is tested in ESI, its child owner is loaded before the zero branch, and EDX holds the corrected count. The retained source mutates the argument, loading the child only after the zero branch. Four complete-TU controls cross an explicit post-debug child owner with a separate corrected-count local. Baseline is 902f47fa, historical headers and immutable production-before. No flags or unrelated bodies change. Prediction: the crossed cell restores the child-load boundary and input/count split; strict byte identity is required. Failure closes this family, regardless of a smaller gap. Full metadata/data/bystander audit follows any winner.

The first owner-cell compilation incorrectly named call_dp rather than the pulse view struct call. That invalid run is retained under build/gcc3-batch50-pulse-digit-invalid-owner-type and excluded; the complete corrected domain is rerun.
