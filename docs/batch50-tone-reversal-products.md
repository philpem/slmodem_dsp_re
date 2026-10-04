# Tone reversal sample carriers and energy update boundary

Declared four complete fpm_tone.c TUs on902: captured unsigned short current
and outgoing history samples, with signed narrowing at product uses, crossed
with separate energy add then subtract statements. Blob movzwl current/old
loads followed by movswl at products, carries current across correlation
clamping without HI spill; source direct repeated subscript loads let current
be spilled atframe+22. Blob energy adds current squared then subtracts outgoing
square (0xab302/304); source forms their difference before adding energy.
Capture only product inputs; final hist write rereads samples[n] after age
stores, exactly as blob. No counter/signature/register declarations adjusted
otherwise. Coefficient/state/period/clamps unchanged. Full TU rawbaseline,
initial RTL/value read boundaries, actual profile/bugdefine/assembler and
metadata/nontext/relocs/data/bystanders. No fuzz/mutation/harness runs; parent
batch finalphase. Close four-cell source family onmiss.

Measured result: Four complete TUs, no exact gain: splitSIZE36, captureSIZE20, bothSIZE15 vsbaseline547B/BYTES463. All seven exact bystanders preserved; no adoption.
