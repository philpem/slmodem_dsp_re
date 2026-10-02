# Notch: a recovered addition tree is not byte identity

Baseline2be7a9e8. At0xaf174 the blob adds state[1] to the feedback product;
at0xaf177 it adds the feed-forward product. Retained source adds the products
first and state[1] last (ours+0x22/+0x24). This differs in arithmetic grouping,
not merely register assignment. F1411's algebraic transfer-function formula
does not establish the finite-precision grouping of these additions.

The predeclared two-cell complete-TU domain tests unchanged Notch.c and
`state[0] = coef[0] * in + (coef[1] * w + state[1]);`. Keep input gain,
recursive node, state-store order, return, types and profile fixed. Both emit
54 bytes against the blob's56, SIZE2. The candidate changes the operation
tree as predicted, but coefficient-load/x87 exchange order still differs.
One strong function/global survives both cells, raw unchanged control
reproduces, no exact gain or loss. No source adoption for this partial result;
the source/grouping domain is closed without another permutation sweep.

[Predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945111290).
Replay `python3 tools/playbook_notch_grouping.py --domain URL`; artifacts in
`build/playbook-notch-grouping/Notch/` include both complete objects, dumps,
full compiler commands/source/header hashes and the changed disassembly.
Shared Gentoo GCC3.4.2-r2 config, selected assembler2.15.92.0.2 and mandatory
bug reproduction are recorded. No flags, floating tolerances, fuzzing or
mutation execution. No differential claim for the unretained candidate.

The next discriminator is a compiler-pass explanation for the distinct
coefficient load/exchange sequence, with the evidenced addition tree held
fixed. Do not introduce cached coefficient locals just to place registers;
the load order alone does not recover their original declarations.
