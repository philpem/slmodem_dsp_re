# V.PCM datapump: the `vpcm.c` definition order, and one displaced register rename

## The fact

`vpcm.c` is one input file in the blob (one `STT_FILE` record). The object
emitted its functions in this order:

| symbol | blob `.text` | size |
|---|---:|---:|
| `vpcm_create` | 0x3a00 | 969 |
| `vpcm_delete` | 0x3dd0 | 110 |
| `vpcm_run` | 0x3e40 | 1,662 |
| `dp_vpcm_init` | 0x44c0 | 72 |
| `dp_vpcm_exit` | 0x4510 | 70 |

The `.data` `vpcm_op` table independently names the same three entry points in
that order (`create`, `destroy`, `process` = 0x3a00, 0x3dd0, 0x3e40), and
`src/pump/v90/vpcm.c` already documented those addresses in its table comment.

Our source defined `vpcm_run` first (line 109), then `vpcm_create` (560),
`vpcm_delete` (723), `dp_vpcm_init` (768) and `dp_vpcm_exit` (785); the object
mirrored that order (`run` 0, `create` 0x680, `delete` 0xa40). GCC 3.4.2 emits
functions in definition order within a TU, and `ld -r` preserves each input's
internal order, so this was a real source-level order difference, not a
manifest artefact.

Moving the `vpcm_run` definition (with its comment) below `vpcm_delete`
produces the blob's order:

| symbol | candidate `.text` after the move |
|---|---:|
| `vpcm_create` | 0 |
| `vpcm_delete` | 0x3c0 |
| `vpcm_run` | 0x430 |
| `dp_vpcm_init` | 0xab0 |
| `dp_vpcm_exit` | 0xb00 |

The bodies do not change: `vpcm_create` 237/241 instructions (LCS 203, 84.2%),
`vpcm_delete` 30/30 (100%), `vpcm_run` 416/426 (LCS 287, 67.4%),
`dp_vpcm_init` and `dp_vpcm_exit` 100% LCS. The two bodies that remain SIZE are
unchanged by the move.

## The displacement

One symbol changes grade: `dp_vpcm_exit` EXACT -> REGALLOC. Its 15
instructions still score 100% LCS; six bytes differ because the three
`modem_dp_deregister` call-site registers rotate. The cause is
`-frename-registers`: once `vpcm_run` moves ahead of `dp_vpcm_init`/`exit`, the
per-register recency cursor entering that pair starts one tick later, so the
same three values receive a different callee-saved register. This is the
cursor-loss class the phased policy records rather than chases.

Measured on the reorder (base `366eb7f0d`):

| metric | before | after |
|---|---:|---:|
| per-TU EXACT | 1,146 | 1,145 |
| REGALLOC | 36 | 37 |
| grade 0-or-1 | 1,182 | 1,182 |
| link-selected EXACT | 1,149 | 1,148 |
| positioned reference bytes | 72,053 | 72,445 |

The ownership decision was to keep the source order (it is the blob's, and
phase 3 is the end target) and displace the one rename through
`tools/ci/displaced.json`.

## Recovery

The cursor loss is expected to close when `vpcm_run` (10 instructions / 5
bytes) and `vpcm_create` (18 bytes) reach their object bodies: the
`-frename-registers` cursor then enters `dp_vpcm_init`/`exit` in the object's
state. Re-measure `dp_vpcm_exit` after either closure and remove the ledger
entry when it is EXACT again; a stale entry fails the ratchet by design.

## Apparatus note

A name the link defines once has no independent phase-2 identity: its
link-selected body is its only body, so its selected membership equals its
per-TU membership. The ratchet therefore checks the selected pin only for
multiply-defined names, and the `exact` entry above carries this loss. Three
self-test cases cover the rule (`--ratchet-self-test`, 31 cases): a
single-definition exact+selected loss displaced by one exact entry; the same
loss still failing without an entry; and a multiply-defined selected loss
still requiring its own entry.
