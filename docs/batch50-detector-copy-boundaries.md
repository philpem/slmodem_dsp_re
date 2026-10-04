# Detector creator copy and setup boundaries

Full Detector.c raw902f47fa. Blob create_dtmf result remains inEAX across the TONEamode_CFG struct copy and is stored to owner+4 only after copy, before TONE_CFG prototype fetch. Retained owner store precedes copy, permitting prototype fetch before REP MOVSL. A local returned child can preserve the witnessed copy-before-store read boundary. Blob setup zero store at+0x28 precedes+0x20; source writes tone thenw6. This order alone is weak scheduling evidence, crossed rather than assumed source.

Four cells declared: retained, staged returned child across configuration copy, w6-before-tone setup, both. No callback/table/count/arithmetic change; all seven named data objects remain. Require full raw baseline, metadata/nontext/relocations and complete bystanders; no exact losses. Adopt only full exact winner with clear recorded boundary, otherwise close both axes.

Fourvalidcells remainSIZE6, nogains/losses. FullTUaudit preservesallsevenobjects andallbystanders. Closebothaxes, noadoption.
