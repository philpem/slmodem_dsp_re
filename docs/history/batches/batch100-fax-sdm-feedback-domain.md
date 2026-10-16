# SDM register feedback before input XOR

Pinned856c1ecb fullSDM.c. Originalscrambler shifts bothsameunsignedregister taps beforeinputload/XOR0x9f190..1ad; baseline completes shift1/inputXOR beforesecondshift. No arithmeticassociationdifference forintegerXOR,localregisterunescaped. Predeclare baseline versus `(reg>>shift1)^(reg>>shift2)^*data` inscriptliteralorder only, crossing independentdescrambler samefeedback-firstexpression withsavedin. Fourcells, nopointercaptureorder/maskwidth/compounddestination changes. Priorbatch50compounddescramblercontrol distinct andclosed,not repeated. Explicitfunctioninsnsreflecttapfeedbacktemporarybeforeinput,notuniqueregisterclaim. Entire3body/TUmetadata/data/relocs rawcontrol; noadoptionbysize orperfileflags.

## Measured outcome

- baseline: all symbol verdicts unchanged.
- scrambler-feedback-first: all symbol verdicts unchanged.
- descrambler-feedback-first: all symbol verdicts unchanged.
- both-feedback-first: all symbol verdicts unchanged.

No new exact symbol. No source adopted. Raw baseline and all complete compiler commands, period identities, source/hash inputs and RTL dumps are retained in `build/gcc3-batch100-fax-sdm-feedback`. Complete TU audit includes metadata, named data, canonical nontext relocations and every function, including nonexact bystanders.
