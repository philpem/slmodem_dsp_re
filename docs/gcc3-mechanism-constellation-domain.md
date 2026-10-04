# Constellation mask unsigned clearing counters

Pinned9f1199b5 (PR261), immutable300 period objects from adjacent
byteexact-batch100/build/tc_out. Four full-TU source overlays independently
cross unsigned clearing counter in getConstellationMask and
getCodecConstellationMask. Original both zero eight words with CMP7/JBE;
retained int i emits CMP7/JLE. All valid counter values0..8 are identical;
the pre-existing signed which clamp, source statements, field owners and
unsigned data-loop n remain fixed. Original distinctIndex has an extra MOV
copy before table scaling; current source already consumes that index for
both size and pointer, so register copy alone does not justify a fabricated
lifetime domain. No index/cursor/store permutations or header/flag edits.
Require raw full-TU baseline reproduction, full metadata/named data/nontext/
relocation and all bystander audits; no source adoption solely by size.
Stop counter domain on a miss. No runtime/fuzz/mutation execution.

Result: all four objects keep4/11 exact functions. Ordinary130B/BYTES38→37,
codec130B/BYTES40→39, no gain/loss. Both signed clearing-loop branches become
original unsigned JBE; all remaining byte differences persist. Full-TU
consumer setV92CPpckFromParamsInfo changes precisely JLE→JBE at instruction95
(ordinary) and142(codec), unchanged230 instruction sequence and operands,
proved separately in each isolated cell and combined cell. Other eight
canonical bystanders unchanged; all named data, allocated nontext sections,
relocations and symbol metadata preserved. Initial RTL GT→GTU counter
predicates fire in2/2 known controls. Complete4/4 TU audit passes. Close
unsigned clearing-counter family without index/scope/spelling variants.
No production source/header changes or runtime execution.
