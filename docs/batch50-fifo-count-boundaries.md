# FIFO8 count result and index update boundaries

Revision902f47fa, retained full Gentoo production configuration, mandatory
bug define and actual selected assembler. Four complete-TU controls: unchanged,
unsigned count result, postincremented element index, both. Consistent candidate
fifo8.h declarations accompany unsigned result definitions. No production header
change before whole-consumer review.

Blob FIFO8_write copies its zero-extended count into EAX; the current signed-short
return emits MOVSWL BP,EAX. The source family is not uniquely typed by this:
unsigned-short and int results can agree when the count already lives zero-extended.
The original caller in Tx.c narrows AX; use that caller as an independent control.

Blob computes and narrows the next index before the byte store while using the
old index for addressing. The current separate store/increment leads to an
if-converted setb/neg/and wrap. Test `buf[wr++] = *src++` and the corresponding
read expression while preserving unsigned16 wrapping and subsequent bounds check.
Prediction: the update placement and result width recover independent parts of
the body; the combined control must be strict exact, not merely equal size.
Falsifier: changed named data/bystanders/consumer behavior or no exact gain closes
this finite source family; no counter/declaration/padding expansion by score.
