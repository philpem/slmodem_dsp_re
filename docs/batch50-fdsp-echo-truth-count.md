# FDSP echo arithmetic count of captured truth

Threecells raw902, capturedquiet/nativepeakowner/unsignedquiet seed, and
arithmetic increment `quiet += low != 0` instead of if(low)quiet++. Blob
setbCL followed separately testECX/setneAL and add toquiet; seed compiles
conditionalincrement to cmp1/sbb. Explicit truth accumulation is the normal
source statement witnessed by blob, preserving0..160 counter and effectorder.
No type/options/permutations beyondpriorseed; fullTU data/nontext/relocs/
metadata/allbystanders. No runtime experiments.

Measured result: 3cells arithmetictruthcount restores359B, BYTES131 remains, no gain. CompleteTUaudit151/151 includes everycell and all nonexactbystanders; no adoption.
