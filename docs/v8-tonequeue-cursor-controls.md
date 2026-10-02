# V8 tone-queue output cursor control

At e70c56b9, v8_TONEq_generate is79B against90B reference. The blob writes
through an advancing output pointer and counts3 down to0; retained source
indexes out[i] and keeps i increasing. Since output addressing is the index's
only use, pointer traversal can make the source index dead and permit automatic
loop reversal. An explicit source countdown is not independently recovered.

The [declared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5955958654)
changes only out[i] to *out++. Int increasing counter, phase arithmetic,
owner loads/stores, unsigned-byte cast and cosine prototype remain unchanged.
Four stores stay within the valid output array; final pointer is one-past.

Production raw-reproduces. Cursor gives direct output store/ADD2 and the
period loop dump reports reversal, while baseline does not. Candidate94B
remainsSIZE4; alpha instruction counts26 versus25 reference also fail. The
extra byte normalization before the cosine call survives. Exact2/9 unchanged,
no gains/losses. Two valid full-TU compiles/two distinct raw emissions use
actual saved flags, DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and executed
assembler2.15.92.0.2.

Only tonequeue and its inlined v8handshak consumer change. Handshake stays
3288B/SIZE955 with743 instruction rows on both controls, but344 canonical
body bytes differ and alpha against baseline fails a branch operand. Both
have56 relocations with identical target multisets; their offset maps differ.
This is not a clean register-only bystander. All seven other bodies and their
canonical relocations agree. Metadata/imports/exports and one named coefficient
object agree. Raw anonymous .rodata41 table addends shift+16 with handshake
placement; each still resolves to the same unique function-interior offset.
All nonrelocated bytes/other allocated nontext agree. Preserve actual raw
layout changes rather than describing all data/relocations as unchanged.

    python3 tools/playbook_v8_tonequeue_cursor.py --domain DOMAIN_URL

Artifacts: `build/playbook-v8-tonequeue-cursor` (actual commands/RTL, complete
symbol/data/jump-table audit and changed-handshake audit). No source adoption,
new fixture execution or new differential claim. Existing fixed fixture covers
64 paired filled component owners,256 output samples and whole-owner checks,
242177 checks including a positive tone witness. This is synthetic component
fidelity, not constructed public handshake history.

Separate ABI observation: all eight blob cosine call relocation sites pass
full arithmetic registers, but the exact14B callee consumes only the low
argument byte. The rounded index can reach256; consumed byte0 is identical
to explicitly normalized0. Upper stack bits do not uniquely establish a wider
formal, an old-style prototype or another source type. Keep the byte API and
casts until complete caller/initial-stage evidence distinguishes a source
boundary. That is outside this pointer domain.

Close the finite output-pointer family without countdown/counter-width/
declaration/register/arithmetic synonyms. F11654 records this negative result.
