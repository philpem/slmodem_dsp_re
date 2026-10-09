# V34 transmit-rate fallback owner

Baselineb3665999. The RX-rate four-cell result is independently exact only
with eager pointers and shared result. Separately, original TX-rate fallback
loads object+0xaa88 directly, with no ratecfg-base LEA or saved EBX. Source
captures &obj->ratecfg at entry and keeps that base for its single txbits use.
Use the ordinary obj->ratecfg.txbits expression instead. This asks whether an
unnecessary long-lived subobject owner explains the original frame difference,
not whether changing arithmetic constants/widths can force a match.

Four full-TU cells cross RX retained/exact control with TX captured/direct
owner. Preserve every TX guard, session capture, zero-result branch, float
arithmetic and bug/prototype/field types. Do not try pointer-lifetime synonyms
if this control fails. Review all bodies/exports/nontext allocated data/BSS and
relocations, original instructions and gain/loss sets. Raw baseline and repeated
RX-only control required. No mutation/fuzzing execution; source gate at batch end.
