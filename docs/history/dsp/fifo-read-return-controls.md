# FIFO8 read return-width control

Atfcf0427a, FIFO8_read is160B versus173B reference. The reference returns
the saved zero-extended count with a32-bit MOV; current short result emits
MOVSWL. The sole production caller explicitly narrows to unsignedshort,
and the existing fixture independently declares the reference result short,
so neither consumer proves the original full result ABI.

[Declared three-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954451790):
retained short, unsignedshort and int, with candidate-only matching public
header overlays and explicit return(short)cast removed for wider results.

    python3 tools/playbook_fifo_read_return.py --domain DOMAIN_URL

All three complete-TU controls compile with the saved production command,
DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and selected/executed assembler
2.15.92.0.2. Unchanged production reproduces its raw object. Wider results
remove the sign extension but become159B/SIZE14; baseline is160B/SIZE13.
Unsignedshort and int cells raw-merge: three valid compiles, two distinct
objects. Exactness stays2/4, no gains/losses. Only FIFO8_read changes across
four emitted functions/one named FIFO_CFG data object. Symbols, bindings,
visibility, imports/exports, data bytes/offsets/targets and allocated nontext
bytes are unchanged.

No API/source adoption and no fixture execution follows this negative result.
The return-width family is closed; no additional signedness/cast/register
spellings are justified. The previously closed FIFO8_write controls remain
separate and closed. A full-EAX declaration is an ABI hypothesis, not a
license to infer original return types solely from unused high bits.
