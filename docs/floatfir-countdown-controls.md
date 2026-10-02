# FloatFIR outer countdown control

At361ef919, block FloatFIR::process is287B against287B reference, but
BYTES244. Equal size is not an agreement claim. The reference decrements its
unsigned counter before the first processing iteration and uses UINT_MAX
sentinel comparisons at entry and the outer backedge. Retained source has an
authentic count-zero return, then do/body/while(--count!=0).

The [two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954856361)
retains the explicit zero guard and all history, coefficient, accumulator,
inner-loop and index ownership, replacing only the outer loop with
while(count--!=0). Replay:

    python3 tools/playbook_floatfir_countdown.py --domain DOMAIN_URL

Both complete-TU cells compile with the saved actual C++ command,
DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and selected/executed assembler
2.15.92.0.2. The unchanged control reproduces its raw production object.
The postdecrement cell grows to303B/SIZE16, with no gains or losses.
Only the block process changes across eight emitted functions and zero
named data objects. All other bodies and symbol type/binding/visibility/
imports/exports/allocated nontext controls agree. Two valid compiles and
two distinct raw emissions.

The postdecrement candidate recovers DEC/CMPUINT_MAX at the outer backedge.
It retains the original explicit entry TEST/JE and introduces a second
sentinel test after member loads. That second zero path is redundant after
the first guard but survives the compiler; the reference has one entry test.
This does not justify dropping its authentic zero boundary or adopting a
larger incomplete body. Close this finite counter family without additional
counter/guard synonyms, declarations or register perturbations.

Existing fixed fixture covers216 paired block calls across36 real constructed
filter lifecycles, including zero, one and lengths that wrap history. Those
are apparatus coverage facts, not newly executed validation of this rejected
cell. No new fixture or deciding differential gate is run for the unadopted
source; production remains unchanged. No fuzzing or mutation execution is used.
Historical F2902 count-up census refers to an older source shape; it is not
a prior measurement of the current do/predecrement versus while/postdecrement
domain. Artifacts and complete-object audit remain under
build/playbook-floatfir-countdown.
