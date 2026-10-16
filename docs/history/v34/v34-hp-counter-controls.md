# V34 timing high-pass counter control

At065e39c0, V34TimingHPFilter is70B against76B reference. The reference
increments the loop index then sign-extends its lowword and compares it as a
signed word against39 (0x27). Retained source declares int k and emits INC
with a32-bit comparison.

[Declared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954962774):
production and conventional short k only. The issue's abbreviated CMPW27
means the disassembled hexadecimal immediate0x27, not decimal27.

    python3 tools/playbook_v34_hp_counter.py --domain DOMAIN_URL

Both complete-TU controls compile with the actual saved production command,
DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and the selected/executed assembler
2.15.92.0.2. Unchanged production reproduces its raw object. The short counter
recovers LEA increment, MOVSWL narrowing and CMPW/JLE, reaching76B but
remaining BYTES29. Alpha comparison also fails: prologue instruction order
already differs at row1, push versus xor. This is not a pure register-renaming
match or complete recovery.

Only V34TimingHPFilter changes across26 emitted functions and48 named data
objects. All25 bystander bodies and canonical relocations, symbol types/
bindings/visibility/imports/exports, data values/offsets/targets and allocated
nontext bytes agree. Exact count stays11/26, with no gains/losses. Two valid
compiles and two distinct raw object emissions.

The arithmetic and history access sequence agree independently of the counter:
carry sample is stored while the old history becomes next carry; coefficients
are signed words, accumulation wraps at32bits, and the Q16 rounded result
shifts by16. The counter remains0..39 in either source. These facts support
the width test but do not authorize changing accumulators, store order,
declarations or registers after its negative result.

Existing fixed apparatus supplies3000 paired high-pass calls and123000 scalar
checks over a deterministic component sample stream. This is coverage evidence,
not a new fixture execution or modem lifecycle claim. No source adoption or
new differential gate is performed for this rejected control. Production
remains919/1852 exact. No fuzzing or mutation execution is used.

The preceding read-only screen inspected three functions in this TU:
high-pass, EchoFilter and EchoAdapt. EchoFilter/Adapt pointer walks and
countdowns remain separately open leads; compiler loop reversal must be
distinguished from genuine source traversal before any new finite test.
Close this high-pass counter family. Artifacts and complete-object audit remain
under build/playbook-v34-hp-counter.
