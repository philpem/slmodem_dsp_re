# V8 DFT energy cursor recovery

At 20821d7c, v8_dftenergy is 98B against the blob's 74B. The blob reads real
and imaginary words at bin+4/+8, writes energy at +12 and advances a bin
cursor by 16. Production indexes bin[i], computing and spilling a byte offset.

The [declared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5955317432)
retains production and tests the conventional advancing-bin loop only:

    for (i = 0; i < n; i++, bin++) {
        /* existing arithmetic, with bin->re/im/energy */
    }

Signed-short i, n and shift, input load widths, multiplication, shifting and
energy narrowing remain unchanged. Nonpositive n still executes no body;
positive n requires a valid array of that many bins. No API contract is widened.

The valid baseline raw-reproduces production. The cursor cell is 74B EXACT;
only v8_dftenergy changes across all three functions and one named data
object. Exact count rises 1 to 2/3, with no losses. Both siblings, canonical
relocations, types/binding/visibility, imports/exports, cosine table bytes and
offsets, and allocated nontext controls agree. Gentoo GCC 3.4.2-r2,
executed assembler 2.15.92.0.2 and the complete saved flags including
DSPLIB_REPRODUCE_BUGS decide this measurement.

    python3 tools/playbook_v8_dftenergy.py --domain DOMAIN_URL

An initial setup failed before any compilation because the shared experiment
driver's manifest omitted src/v8/v8int.h. This is explicitly invalid, preserved
under `build/playbook-v8-dftenergy-invalid-local-header`, and excluded from
codegen evidence. The driver now registers tracked local headers from each
source directory as well as the retained V32 register and rejects baseline
header drift. The valid rerun records the V8 header and passes raw baseline
reproduction. F11648 records this apparatus correction; it changes no flags
or reconstruction behavior.

Production adoption inspects all 300 period objects: only src_v8_V8Dftc.c.o
changes, raw-identical to the measured candidate. Whole-tree 919 to 920/1852
exact, 94,776 to 94,850 exact bytes, sole gain v8_dftenergy and no losses.
The fixed Gentoo make phase gate passes 386 tests, zero failures, plus all
structural gates. Static anchors: 285 suites / 10038 anchors, none detached
or nonunique. Existing t_v8util energy coverage passes 8705 checks over 272
paired calls (shifts 0..15, counts 0..16), checking energy and untouched pads.
The production caller in V8.c uses count=1, shift=1. No fuzzing or mutation
execution and no new fixture are needed.

Complete same-order 300-object partial links remain DIFFERENT, strict exit1
before and after. Positioned equal bytes 68,545 to 68,604 / 943,398 (+59);
allocated bytes 914,814 to 914,782 (-32). Exact section records 70/92 and
symbol records 394/2907 stay unchanged; relocation records 1025 to 1023/18317.
Report this layout effect alongside the function gain; this does not recover
the full original object/profile.

Artifacts: `build/playbook-v8-dftenergy` (commands, RTL, results, full TU audit)
and `build/playbook-v8-dftenergy-adoption` (all-object promotion, census and
complete partial links). F11647 records the adopted source recovery.
