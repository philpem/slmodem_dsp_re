# SGD sequence output cursor and old count

Pinned856c1ecb fullTU. Original sequence_gen captures old output pointer and advances cursor before active test0x9f7df..ea; current does both after selecting symbol. OriginalcountINCword/JNE0x9f789..78f/7d5..dd supports while(n--) as in symbol generator, not current explicitsentinel. Four cells crossing count condition with outputdestination capture: unsigned short *slot=out++ beforeactivebranch, selectedsympublishedthroughslot afterward. No moved input/state reads or altered short index/enable/repetition guard/return. No volatile/spillinventing or loopwidthchange; sharedtype/home unchanged. Fullrawbaseline/all9TUfunctions/metadata/data/relocs review, no sizeadoption. Previous correlate/patterncountfamilies not rerun.

## Measured outcome

- baseline: all symbol verdicts unchanged.
- output-before-select: {'SGD_sequence_gen': ['SIZE', 16]}.
- postdecrement: all symbol verdicts unchanged.
- output-before-select-postdecrement: {'SGD_sequence_gen': ['SIZE', 16]}.

No new exact symbol. No source adopted. Raw baseline and all complete compiler commands, period identities, source/hash inputs and RTL dumps are retained in `build/gcc3-batch100-fax-sgd-sequence`. Complete TU audit includes metadata, named data, canonical nontext relocations and every function, including nonexact bystanders.
