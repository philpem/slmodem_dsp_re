# getFrame fixed groups, callback capture and word locals

Baseline80da7b0c complete src/pump/v34/v34shell.c/current headers, retained
Gentoo flags and mandatory bug define. No flags/profile changes.

Original getFrame1028B (579c0) loads callback at579dd, saves it at579ea and
all six indirect calls use stack+40. Retained597B loads s->scramble directly
at three static call sites; the four groups remain a source loop. Original
four group regions have fixed frame offsets and separate conditional callbacks
(57d24/57d3e/57d60/57d82), not a group backedge. Original nb/pos comparisons
are16-bit (57a24/57a33/57a3d); defaults are narrowed at57a82/57a8b.

Eight-cell cross: explicit four groups, entry callback capture after s owner
construction/before w read, and word local nb/pos/small/small_last. Keep w int.
Maintain each callback's timing, input read/reload and bitpos publications;
wide split deliberately discards callback return as original does. No buffer
capture across callbacks. Prediction: explicit groups restore six static calls;
entry capture makes all calls use one saved callback; word locals restore word
comparisons. Falsifier: all cells miss byte identity or contradict original read
ages. A recovered callback boundary is independent of call-count closeness.

Full raw baseline must reproduce; compare all bodies, bindings, data and
relocations, including nonexact caller modulatevector and unchanged decodeDepth.
Previous putFrame/snapshot domains are not reopened. No fuzzing/mutation or
forced registers. Any source adoption needs final period/structural gate.
