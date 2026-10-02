# V8 queue counter-width recovery

At 379d400b both queue helpers are 85B against 94B reference. Their sample
loads/stores, pointer traversal, wrap tests, availability updates and
constant-zero return already agree. The blob increments a counter with LEA,
sign-extends the lowword and compares the word against 3; retained source
declares int i.

The [declared four-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5955537135)
crosses the RX/TX counter widths independently, changing no other statement.

| Counter source | RX bytes/verdict | TX bytes/verdict | Exact TU functions |
| --- | --- | --- | --- |
| int / int (production) | 85 / SIZE9 | 85 / SIZE9 | 3/13 |
| short / int | 94 / EXACT | 85 / SIZE9 | 4/13 |
| int / short | 85 / SIZE9 | 94 / EXACT | 4/13 |
| short / short | 94 / EXACT | 94 / EXACT | 5/13 |

No exact losses. The conventional short counter is adopted in both helpers;
it stays within 0..4, so narrowing changes no valid execution or API.

Initial RTL already distinguishes the sign extension. The loop pass reports
"Can reverse loop" and "Reversed loop and added reg_nonneg" for both int
helpers; neither short helper is reversed. Thus the reference's forward word
counter can be recovered through source width, without disabling a pass or
forcing a register. Keep the stage evidence instead of inferring source from
size alone.

All four full-TU compiles use actual saved flags, DSPLIB_REPRODUCE_BUGS,
Gentoo GCC 3.4.2-r2 and executed assembler 2.15.92.0.2. Production raw-reproduces;
there are four distinct raw emissions. Complete audits cover all 13 functions
and four named data objects: types/binding/visibility/imports/exports, data
values/offsets/targets and allocated nontext bytes agree in every cell.

RX narrowing also changes the inlined queue in V8agc. The 961B body remains
nonexact against 1126B reference (SIZE165). Review covers the entire prefix:
queue counter lowering, local register choices, shifted following filter
setup, alignment padding and its outer backedge displacement. From +0x100
to the end, all 705 bytes are identical; all nine canonical relocation records
are identical throughout. Do not call the bystander register-only. TX-only
changes only its helper. Combined changes exactly the two helpers and V8agc;
all ten other bodies and relocations agree.

    python3 tools/playbook_v8_queue_width.py --domain DOMAIN_URL

Artifacts: `build/playbook-v8-queue-width` holds commands, RTL, body results,
complete symbol/data/nontext audit and V8agc suffix audit.
`build/playbook-v8-queue-width-adoption` holds production promotion, census,
fixed gate and complete same-order partial links. F11649 records the recovery.

Existing t_v8sig constructs 40 paired component objects with ref_v8_txinit on
both sides, then performs 155 paired calls to each queue (one to seven blocks
per object). Pointer offsets are compared and normalized before whole-state
comparison, passing 151671 checks. These are initialized component histories, not claimed public
handshake reachability; no new fixture, fuzzing or mutation execution.

Production promotion inspects all 300 period objects: only V8global changes
and raw-matches the combined cell. Whole-tree 920 to 922/1852 exact, 94,850
to 95,038 exact bytes, sole gains the two queue helpers with no losses.
Fixed Gentoo phase passes 386/386 and all structural gates; static 285 suites /
10038 anchors, none detached/nonunique. Complete same-order partial links
remain DIFFERENT (strict exit1 before/after): positioned matches 68,604 to
68,596/943,398 (-8); allocated bytes 914,782 unchanged. Exact section records
70/92, symbols 394/2907, relocations 1023/18317 unchanged. Alignment absorbs
the helper growth; report the positioned loss alongside the two body gains.
