# Phasor original-word consumption controls

[Four-cell predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5948379720)
at04d3177a crosses signed phase versus (unsigned short)phase as left operand
of phase-(idx<<5), and int versus short frac. Signed index/quadrant, output
pointers, callers, tables/sign extensions and phase advance unchanged.

Full production baseline raw-reproduced; prior short-frac cell raw-replays
F11589's complete object. Four sources/three emissions. Baseline oscillator
sizes199/142/233B (phasor/demod/dp); short200/143/234B; unsigned204/147/238B;
combined200/143/234B raw-identical short-only. Blob211/161/246B. No exact gains,
0/3 unchanged. GCC discards the unsigned operand's additional bits before
short assignment; initial remainder still and1f, reflected movswl retained.
All5 functions (3 blob-shared)/9 global records/nontext/type/binding/visibility
preserved; only three oscillator bodies change, two extra entry helpers unchanged.

No source adoption or candidate runtime/retained census/partial gates RUN.
Existing exhaustive phase tests not claimed as candidate validation. Full
Gentoo saved configuration plus mandatory bug define, executed selected
assembler2.15.92.0.2; tool playbook_phasor_consumption.py and artifacts
build/playbook-phasor-consumption contain actual commands/hashes/inventories,
all changed disassembly/RTL and full nontext/symbol audits. F11590.

Close this original-word/local-width source family: the observation motivated
the cross but does not recover the reference source. Do not expand equivalent
casts or register-specific/declaration permutations based on closer sizes.
