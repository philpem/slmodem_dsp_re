# V22 HDX alias-boundary deep dive — 2026-11-05

The completed [DemodDataV22 recovery](demodv22-recovery-20261105.md)
makes the combined candidate EXACT at 510 bytes and removes the alternative's
Demod regression. The owner view restores the two HDX byte scores; its original
declaration remains unresolved. The initial nine-cell diagnostic below explains
why the later caller/callee and conditional-owner corrections were needed.
Plain `void *` does not reproduce the pointer union's alias effect.

## Reproduce the cause before naming the declaration

The landed implementation uses a signed 16-bit HDX state view to preserve
control's late DSP-owner acquisition. An alternative restores ordinary HDX
shorts and moves alias-zero behavior to a union of typed and opaque DSP
pointers at root +0x54. With explicit early/late owner captures, it emits the
same control body. Without those captures it acquires the DSP owner four
times, including immediately after the first descrambler publication.
Therefore that earlier writer can account for the invalidation; the native
late load alone does not identify HDX stores as its original cause.

The status body supplies an independent value-age discriminator. Native code
acquires HDX at 0x8c45a, reads signed protocol at 0x8c45d, acquires DSP at
0x8c461, reads the protocol table at 0x8c464 and publishes protocol at
0x8c46c. The initial DSP owner survives the short report stores through the
first scrambler read at 0x8c4b6. Its first replacement follows the flag-byte
publication at 0x8c4d7. Capturing protocol before the initial DSP owner
recovers this graph; capturing DSP alone leaves eight status-byte differences.

## Nine full-stc cells

All cells use unchanged retained Gentoo flags. BYTES numbers are differing
bytes; SIZE numbers are length differences. Control REGALLOC is raw BYTES11.

| Cell | Control | Status |
| --- | --- | --- |
| Landed control/header | REGALLOC | EXACT |
| Ordinary HDX shorts, typed DSP pointer, control owner captures | BYTES50 | EXACT |
| Pointer union, control captures | REGALLOC | SIZE15 |
| Pointer union, no control captures | SIZE3 | SIZE15 |
| Ordinary pointer, early status DSP capture | BYTES50 | BYTES8 |
| Pointer union, early status DSP capture | REGALLOC | BYTES8 |
| Ordinary pointer, protocol then early status DSP capture | BYTES50 | EXACT |
| Pointer union, protocol then early status DSP capture | REGALLOC | EXACT |
| Plain void pointer, typed casts, same native captures | BYTES50 | EXACT |

The eighth cell's whole object is raw-identical to the landed object. In the
plain-void control, the owner access retains alias set 16 and only one DSP
acquisition survives. The pointer union changes that access to alias set zero
at expansion. Ordinary HDX state stores retain short alias set 3. The union,
rather than generic pointer spelling, causes the effect. An unused opaque
member is insufficient evidence for an original owner declaration.

## Seven-consumer closure

Seven paired builds add fourteen compilations: 23 total with the nine cells.
All seven unchanged controls raw-reproduce their copied landed objects. Six
code TUs contain 44 shared functions; v22rxtab is data-only and its complete
object remains raw-identical. The zero-function TU is assessed by hashes,
not silently omitted from the denominator.

| Consumer | Functions | Alternative versus landed |
| --- | ---: | --- |
| v22.c | 5 | Unchanged |
| V22.c | 3 | create body changes, SIZE40 remains |
| v22stc.c | 2 | Whole object raw-identical |
| v22prc.c | 10 | Detect_v22 changes, SIZE29 remains |
| V22int.c | 14 | DemodDataV22 BYTES231 becomes SIZE16; other 13 unchanged |
| v22mod.c | 10 | ans_rmloop2 BYTES18→7; org_rmloop2 101→90; answer, data, originate and retrain retain SIZE grades with changed bodies |
| v22rxtab.c | 0 | Data object raw-identical |

Exports and allocated data remain unchanged across all six code TUs. No
EXACT/REGALLOC membership changes occur. Restoring the two HDX byte scores
moves collateral to DSP-owner consumers; it does not establish a narrower
original representation. This is a reason to test supported interactions,
not a veto against native-backed source changes in those consumers.

## Evidence limits and continuing experiment

The initial nine-cell diagnostic did not receive period/phase validation.
The coherent combined source receives its own integration gate, recorded in
the Demod report; that verdict must not be attributed to every diagnostic cell. Existing request
and status overlaps establish value ages; they do not prove replacement of
the DSP owner through a valid modem history. Constructor-owned DSP and HDX
allocations are distinct. A planted overlap changing pointer bytes would be
an explicitly synthetic exploratory probe. Here the native/retained literal
object agreement supplies owner-epoch evidence.

No format string or type-bearing symbol establishes a pointer union at root
+0x54. Reconstructed V22_OBJ_FP/V22_OBJ_GTIMER labels are not original author
type evidence. The neighboring V32 options.retrain diagnostic establishes
neither these HDX state fields nor this DSP-owner declaration.

The completed interaction crosses the header alternative with native-supported
DemodDataV22 owner/value captures: baseline, header alone, source alone and
both. Map every proposed capture against native calls and publications, review
all fourteen V22int functions and then all seven header consumers. The selected
combined source is subject to the period differential/phase gate. Do not add
an opaque member solely to obtain preferred alias sets or registers.

Stop another declaration palette unless new original-owner/aggregate evidence
or a native-backed access boundary predicts the divergent consumers. At the
end of the combined experiment, update the Playbook with the mechanism,
failed controls, collateral and reopening condition. On the next recovery turn,
check the new lesson against another independently witnessed function.

Artifacts: `/mnt/zfs/dev/slmodem/scratch/v22-deep-20261105/owner-cause/`.
`summary.json` records captured source/header/object hashes, compiler identity,
flags and cell results. Cell command records are executed commands; closure
candidate commands are explicitly reconstructed from the hashed executed
generator. The compiler image is the retained Gentoo image, GCC 3.4.2-r2,
with selected GNU assembler 2.15.92.0.2. [Issue #590](https://github.com/philpem/slmodem_dsp_re/issues/590)
tracks the continuing investigation.

## Combined integration

The selected coupled source passed `make phase J=8`: 399 period tests,
zero failures and all structural checks. Its pointer-union view replaces the
initial HDX state view as an explicit alias-contract model, with the original
declaration still unresolved. The full 300-object review and remaining
non-exact collateral are recorded in the Demod report. This successful combined
gate does not validate the earlier isolated diagnostic cells.
