# V34 bitreverse initial predicate control

Atfcf0427a, bitreverse is56B against64B reference. The reference initializes
the result through zero/TEST/SETNE, while the retained ternary becomes MOV/AND1.
The substantive loop already agrees; caller open-coding findings F11525/F11526
address a different boundary.

[Declared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954569313):
production and ordinary out=0; if(v&1)out=1, with types, loop and API retained.

    python3 tools/playbook_v34_bitreverse_init.py --domain DOMAIN_URL

The saved baseline is the unchanged pre-transmit-queue V34TX object in
build/playbook-v34-txqueue-adoption/before. This is necessary because
production separately adopted the queue recovery during this diagnostic.
Source/header revision and actual full saved compiler command stay explicit;
the replay checks the baseline object byte-for-byte. Gentoo GCC3.4.2-r2,
DSPLIB_REPRODUCE_BUGS and executed assembler2.15.92.0.2 are retained.

The statement reaches64B but remains BYTES40: TEST/JE/MOV rather than the
predicted TEST/SETNE, with changed register allocation too. Equal size does
not recover the predicate or the body. Two valid compiles, two raw emissions;
exact count0/7 unchanged. Only bitreverse changes across seven emitted
functions/zero named data objects. All other bodies and symbol type/binding/
visibility/import/export/allocated nontext controls agree.

No source adoption, new fixture or runtime claim follows. Existing t_v34rx
supplies37212 paired scalar checks (21 counts,1772 sampled input words),
not an exhaustive input domain. Close this statement-predicate family without
boolean synonyms, declaration/register perturbations or size acceptance.
