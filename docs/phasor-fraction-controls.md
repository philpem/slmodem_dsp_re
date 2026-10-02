# Phasor interpolation fraction-width control

[Predeclared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5948298874)
uses b3d741f7's unchanged phasor TU. Reference demod161B narrows the
phase-(idx<<5) remainder at0xa9513 and reflected32-frac at0xa9521. Change only
phasor_split's local frac from int to short, preserving int output pointers,
callers, all helpers, phase advance and extended sign tables.

Production complete object raw-reproduced. Short-local cell adds one byte to
all three oscillator bodies: FPM_phasor199→200B vs blob211B; demod142→143B
vs161B; dp233→234B vs246B. No exact gain,0/3 unchanged. Both source cells
produce distinct complete objects; all5 defined functions (3 blob-shared)/
9 global records, bindings/type/visibility/data preserved; two extra entry
helpers absent from blob unchanged. The reflected fraction now has movswl AX,ECX, but the initial
remainder still folds to and1f. This local type alone does not recover
reference conversion/control boundaries; no source adoption or declaration/
register permutations to fit the residual.

No candidate runtime/retained whole-tree/partial gates RUN. Existing exhaustive
phase fixtures are not claimed as validation of this unadopted candidate.
Full saved Gentoo profile plus mandatory DSPLIB_REPRODUCE_BUGS, executed
selected assembler2.15.92.0.2; tool playbook_phasor_fraction.py and artifacts
build/playbook-phasor-fraction retain commands, hashes, inventories,
nontext/symbol audits and all three changed disassemblies/RTL. F11589.
The separate F8322 phase-advance remainder cast is not this boundary.

Next discriminating observation: reference first loads the original word
zero-extended at0xa94f6, makes a signed copy at0xa94f9 for the shifted index,
and subtracts from the original zero-extended word. Current source uses one
signed int phase for both roles. That separately observable consumption
boundary is untested; local frac width results do not settle it. Preserve the
sign-table extension/data and predeclare any such subsequent cross separately.
