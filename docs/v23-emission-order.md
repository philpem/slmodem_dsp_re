# V.23 datapump: the `v23.c` definition order

## The fact

`v23.c` is one input file in the blob (one `STT_FILE` record, between `v32.c`
and `v22.c`). The object emitted its functions in this order:

| symbol | blob `.text` | size |
|---|---:|---:|
| `v23_create` | 0x4c30 | 274 |
| `v23_delete` | 0x4d50 | 72 |
| `v23_process` | 0x4da0 | 464 |
| `dp_v23_init` | 0x4f70 | 30 |
| `dp_v23_exit` | 0x4f90 | 28 |

Our source defined `dp_v23_init` first, then `v23_create`, `dp_v23_exit`,
`v23_delete` and `v23_process`, and the object mirrored that order (`init` 0,
`create` 0x20, `exit` 0x140, `delete` 0x160, `process` 0x1b0). GCC 3.4.2 emits
definitions in definition order within a TU, so this was a source-level order
difference rather than a manifest artefact.

Moving `dp_v23_init` and `dp_v23_exit` (with no comment blocks of their own)
below `v23_process` gives the object's order:

| symbol | candidate `.text` after the move |
|---|---:|
| `v23_create` | 0 |
| `v23_delete` | 0x120 |
| `v23_process` | 0x170 |
| `dp_v23_init` | 0x350 |
| `dp_v23_exit` | 0x370 |

The move changes no body. Before and after, `v23_create` is 76 instructions
(LCS 74, 97.4%), `v23_delete` 20/20 (100%), `v23_process` 137 against the
blob's 135 (LCS 94, 68.6%), `dp_v23_init` 8/8 (100%) and `dp_v23_exit` 7/7
(100%). Their per-TU grades are unchanged: `v23_create` BYTES, `v23_process`
SIZE, and `v23_delete`/`dp_v23_init`/`dp_v23_exit` EXACT.

## Measurement

Measured on base `9e34d1da6`:

| metric | before | after |
|---|---:|---:|
| per-TU EXACT | 1,145 | 1,145 |
| REGALLOC | 37 | 37 |
| grade 0-or-1 | 1,182 | 1,182 |
| link-selected EXACT | 1,148 | 1,148 |
| placement order steps (b) | 363 | 358 |
| sequence-aligned names | 1,539 | 1,541 |
| positioned reference bytes | 72,849 | 72,841 |

No symbol changes grade, so this wave records no displacement in
`tools/ci/displaced.json`; the EXACT set is identical before and after.

The eight-byte fall in the aggregate phase-3 number is coincidence, not a
body move. The candidate `.text` is 16,304 bytes shorter than the blob's at
this point, so only 4.7% of `.text` bytes (34,578 of 728,304) match by
position at all, and reordering 385 bytes of the file changes that
coincidence by eight. The targeted order measurements move the right way:
five fewer category-(b) steps and two more sequence-aligned names. Aligning
the absolute `.text` offsets is upstream size work, not this reorder.

## The rest of the family

The other datapump TUs were checked against the same evidence and already
emit in the blob's order, so none was touched:

| file | blob order | our object order |
|---|---|---|
| `v8.c` | create, delete, process, init, exit | same |
| `vpcm.c` | create, delete, run, init, exit | same (PR #361) |
| `v32.c` | create, delete, process, init, exit | same |
| `v22.c` | create, delete, process, init, exit | same |
| `b103.c` | create, delete, process, init, exit | same |
| `dp_init.c` | init, exit | same |
| `dp_param.c` | get, runtime\_create, runtime\_delete | same |
| `dp_wrapper.c` | delete, create, run | same |
