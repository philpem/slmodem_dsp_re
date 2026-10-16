# V8Process publication recovery: finite partial match

Baseline153b22b0, Gentoo3.4.2-r2/assembler2.15.92.0.2, complete retained flags
including DSPLIB_REPRODUCE_BUGS and unchanged headers.
[Domain](residual-v8-cursor-domain.md) recorded before compilation.

| Ring local | Symbol local | Bytes | Strict verdict |
|---|---|---:|---|
| No | No |731| BYTES233 |
| Yes | No |731| BYTES73 |
| No | Yes |731| BYTES233 |
| Yes | Yes |731| BYTES71 |

Original731B publishes each advanced/wrapped pointer once. Ring local removes
the retained duplicated store paths and restores the original forward layout
of the per-sample loop. In initial RTL ring stores go from two to one; that
difference persists in machine RTL. Symbol local likewise changes initial RTL
stores from two to one, but both forms already have one machine publication.
It is therefore a different axis, despite superficially similar source shapes.

Original remains distinguishable: scalar input/pole loads have different order,
symbol bound comparison has different operand orientation, initial zeros use
different registers/slots and the retained candidates still miss the body.
Static duplicated publication paths do not mean two dynamic stores on every
sample or a measured functional bug. Complete-TU raw baseline reproduces;
four cells28common verdicts retain exact2/7, all six bystanders and metadata,
bindings/imports/exports/data/BSS/nontext relocations unchanged. No source/header
adoption or exact gain/loss; the finite cursor cross is closed. No constructor
compensation or new local/register synonyms from the smaller difference score.

Replay tools/residual_v8_cursor_reproduce.py, then
tools/residual_v8_cursor_audit.py. Seven uniquely owned RTL stages per cell;
positive/negative publication counts fire on both axes. Artifacts under
build/residual-v8-cursor; audit in build/residual-v8-cursor-audit.json.
Tools compile, refcheck/diff pass. Production1079/1852 and source unchanged;
previous source period388/0 remains valid. No candidate runtime, mutation or
fuzzing execution; no portability claim.
