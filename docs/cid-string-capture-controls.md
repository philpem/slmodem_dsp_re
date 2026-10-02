# CID string child-capture control

At9565d51c, cid_get_strings is146B against the reference145B. After the
external600-byte sysdep_memset and DTMF mode predicate, the reference captures
ctx->dtmf once before its16-byte loop. Retained source reloads that owner
inside each iteration.

[Declared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954292200):
production and ordinary branch-local const struct dtmf_rx *dtmf=ctx->dtmf,
with dtmf->digits[i]. Replay:

    python3 tools/playbook_cid_strings_capture.py --domain DOMAIN_URL

Both complete-TU controls compile with the saved command, mandatory
DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and selected assembler2.15.92.0.2.
Production reproduces its raw object. The capture restores the pre-loop owner
load, but remains146B/SIZE1. Exact count stays6/10, with no gains or losses.
Only cid_get_strings changes across10 emitted functions; there are0 named
data objects. Symbol types/bindings/visibility/imports/exports and allocated
nontext bytes are unchanged. Both cells produce distinct raw objects.

The receiver is separately allocated by the real constructor. Output
ctx+8..23 cannot overwrite the owner pointer atctx+0; capture stays after
the external clear. The sole production service caller uses its owned CID
object. Existing routing apparatus supplies150 component cases (5 frames,
6 modes,5 values), including forced internal modes; this is not a claim of
150 public lifecycles. No fixture was run for this unadopted control.

Close this typed child-capture domain without expanding pointer synonyms,
declaration positions or register choices. No production source change is
adopted and no fuzzing or mutation execution is performed. The preceding
read-only screen inspected4 targets across19 emitted TU functions: FIFO8
read/write, silence_create and cid_get_strings. FIFO write and silence
families were already closed; FIFO read's full return width remains a separate
ABI hypothesis requiring its own domain and caller review.
