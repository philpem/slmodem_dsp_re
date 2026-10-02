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

## Postreload scheduler discriminator

F11627. [Predeclared four-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5953178844)
at b09cadf7 crosses the same two source cells with retained flags and only
-fno-schedule-insns2. GCC3.4.2 toplev.c runs sched2 before reg_to_stack;
the existing dumps already separate scheduled loads from inserted exchanges.
This is a diagnostic option control, not a proposed production flag change.

Both retained-profile cells remain54B/SIZE2. Both disabled-scheduler cells
are48B/SIZE8, with no exact gain/loss (0/1 common function). Four valid
compilations yield four distinct objects. Unchanged baseline raw-reproduces
production. The one-function/zero-data TU retains all symbol/binding/import/
export and allocated nontext controls. No source or flag adopted.

The mechanism is visible at specific instruction IDs. In feedback-first,
before sched2 the load/calculate sequence is49,15,50,18,51,20. After sched2
it is49,51,50,15,18,20: loads of state[0],coef[1],coef[0] are hoisted ahead
of calculations. Stack conversion subsequently inserts exchange IDs57–60.
With sched2 disabled, the stage dump is absent as expected; stack conversion
preserves the pre-scheduling sequence and inserts only exchange57 at the final
state store. The final load order still does not reproduce the blob's
coef[0],state[0],coef[1] at0xaf15f/0xaf161/0xaf163. Thus sched2 contributes
to load ordering and later exchanges, but disabling it does not explain the
reference. An x87 exchange mismatch cannot be assigned to register renaming
without separating these stages. No cached coefficient or declaration sweep.

Artifacts build/playbook-notch-schedule retain complete flags, bug define,
Gentoo compiler/executed assembler identity, hashes, RTL, disassemblies,
Notch-complete-object-audit.json and stage-instruction-order.json. Replay
with tools/playbook_notch_schedule.py and the domain URL. No new differential
or partial-link verdict claimed; source and production profile are unchanged.
Next evidence would need a different independently established compiler/source
boundary, rather than further scheduling/tuning options selected by score.
