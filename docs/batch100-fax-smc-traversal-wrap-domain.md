# V17 symbol coders counted cursor and conditional ring clear

Pinned856c1ecb fullSmc.c. Original abs0x9fd80..9fddd loads unsignedshort count, word INC/JNE old-value loop, data cursor advance2. Current indexed unsigned-int forwardfor differs. Original ringwrap0x9fdc5..d5 uses signedword comparison SETL/NEG/AND; currenthelper ternary branches. Transfer measured Playbook noce_try_store_flag_mask destination-identity lever: keep shortnext, replacehelperternary with if(next>=len)next=0;return next. Four crossed completeTU cells: baseline; allthree V17 coder forloops→while(count--) with data[i]→*data++ (tcm in-place word writesuse*data,advanceonceafteroutput); helperconditionalclear; both. Countunsignedshort/operandbounds/narrowing,ring/state writes,math/tag/tablemodels unchanged. SMC_encoder unchanged (usesdifferentwrap),nocursorpermutations/sharedheaders. Audit original count/cursorwitness foreachcoder andallfiveTU bodies/data/exports/relocs, rawbaseline and exactinit retained. PreviousV32family negative doesnot establishV17 preimage; stop this boundedcrossif no completehit.

## Measured outcome

- baseline: all symbol verdicts unchanged.
- counted-cursor: {'SMCv17_encoder_abs': ['SIZE', 18], 'SMCv17_encoder_dif': ['SIZE', 1], 'SMCv17_encoder_tcm': ['SIZE', 11]}.
- conditional-clear: {'SMCv17_encoder_abs': ['SIZE', 5], 'SMCv17_encoder_dif': ['SIZE', 2], 'SMCv17_encoder_tcm': ['SIZE', 7]}.
- counted-cursor-conditional-clear: {'SMCv17_encoder_abs': ['SIZE', 13], 'SMCv17_encoder_dif': ['SIZE', 4], 'SMCv17_encoder_tcm': ['SIZE', 4]}.

No new exact symbol. No source adopted. Raw baseline and all complete compiler commands, period identities, source/hash inputs and RTL dumps are retained in `build/gcc3-batch100-fax-smc-traversal-wrap`. Complete TU audit includes metadata, named data, canonical nontext relocations and every function, including nonexact bystanders.
