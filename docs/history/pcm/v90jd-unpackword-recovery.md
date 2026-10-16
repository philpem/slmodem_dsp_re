# V90Jd constructor unpackWord position and constellation read order

Three posted stages ([stage 1](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5966290258),
[stage 2](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5966422741),
[stage 3](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5966422741 amendment)), one adopted gain.
Artifacts: `build/playbook-v90jd-unpackword/`, `build/playbook-v90jd-nocast/`,
`build/playbook-v90jd-temps/results.json`. Same worktree/profile as
F11671-F11674.

## Result

`_ZN5V90JdC2EP13V90Parameters` EXACT at 119 bytes with:

- `unpackWord = 0;` moved after the `look = ...` parameter read (the blob
  stores 0x8c after the movzbl, not with the unpack[] bytes);
- both constellation parameters read into ints before either is stored
  (`int c47 = params->V34_PHASE4_CONSTELLATION; int c48 = ...;` then the
  two byte stores). The scheduler cannot prove `params` and `this` do not
  alias, so interleaved statements make the second load wait on the first
  store and the blob's load,load,store,store shape is lost.

C1 improves BYTES 50 -> 34 (same scratch-register mirror the V92CP ctor
shows; RA-class, not chased). Census 928 -> 929 / 1852, bytes 95,564 ->
95,683, sole gain, zero losses.

## Stages and falsified cells

- Stage 1 (unpackWord position alone): all four positions change the body;
  after-look moves the 0x8c store to the blob's slot but ndiff stays 34.
- Stage 2 (cast removal): `(unsigned char)` removal on the constellation
  reads folds -- the load width follows the field's declared `int` through
  nop casts, and the earlier WIDTH rejection rows were row-desync artefacts
  of the store-position shift, not width differences.
- Stage 3 (adjacent reads): the winning form.

## Records

Five v90jd/v90jdstd mutation anchors retargeted to the adopted spellings
with fault cases preserved (transposed-constellation, whole-word-carry,
unpacker word/second-byte); anchorcheck 285 suites / 10,038 mutations, zero
problems. No differential or fuzz change (value-identical reorder). Fixed
phase gate before commit; the historical --ratchet floor caveat of F11671
applies unchanged.
