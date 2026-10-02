# V8 shaping-filter counter and coefficient-cursor controls

The remaining small V8 screen at afedb41d inspects six functions across four
complete TUs containing24 functions. v8_fsktxfilter is the strongest fresh
source discriminator: repeated signed-word counter narrowing/comparison and
an advancing coefficient pointer in the blob differ from retained int i and
indexed coefficients. Existing compiler output already reverse-walks history.

The [declared four-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5955732340)
crosses int/short counter and indexed/advancing const short coefficient pointer.
Sample insertion, history indexing,61 taps, signed sample multiplication,
accumulator0x8000/update, Q16 shift and return ABI remain. No explicit history
cursor is added: decrementing below the first array element on the final tap
would introduce a pointer-boundary issue merely to fit machine cursors.

| Cell | Bytes | Verdict |
| --- | ---: | --- |
| production | 70 | SIZE46 |
| short counter | 76 | SIZE40 |
| coefficient cursor | 81 | SIZE35 |
| both | 90 | SIZE26 |

Reference116B. Narrowing restores LEA/MOVSWL/CMPW60(0x3c); coefficient
cursor restores direct tap loads and ADD2. Complete bodies miss: reference
alpha count34, candidates25/25/27 (production24). Combined still merges
history read/write into one reverse pointer and keeps accumulator in a
register; blob retains separate reverse pointers and stack accumulator.
Those residual observations do not authorize declaration/register/spill forcing.

All four full-TU cells compile with actual saved flags, DSPLIB_REPRODUCE_BUGS,
Gentoo GCC3.4.2-r2 and executed assembler2.15.92.0.2. Raw baseline reproduces;
four distinct emissions. Only fsktxfilter changes across all four functions /
zero named data objects. Exact2/4 unchanged; no gains/losses. All other bodies
and canonical relocations, symbol metadata/imports/exports and allocated
nontext controls agree. Artifacts: `build/playbook-v8-fsktx-controls`.

    python3 tools/playbook_v8_fsktx_controls.py --domain DOMAIN_URL

Existing t_v8sig fixture has1920 paired calls (24 filled component owners ×
80 samples) and whole-owner comparisons,92640 checks. These are synthetic
component fidelity probes with valid bounded arrays, not constructed public
modem histories. No new fixture execution or candidate differential claim.
No source adoption. This finite width/cursor family is closed; any future
hypothesis needs independent source evidence, not near size. F11652 records
these negative controls; reconstruction phase is not reopened.
