# V21 transmit status alias-visible sampling

902f47fa retained profile. Blob reads tx flags only after protocol/zero/status
stores and first flags clear. Current source snapshots before every store,
so intermediate first flags clear disappears and caller overlap sees a
different result. Relocate exactly that source sample to the observed late
boundary; independently compare before adjacent flags1 clear versus after
it, and bit2 mask before clear versus at final assignment (F10231).
Four late-read cells plus original baseline; no unrelated store changes,
local type variants, header or profile edits. Bit0-only adjacent clear cannot
change admitted bit2, so the two late sides are equivalent for overlaps;
early read is a separate historical source boundary. Complete TU audit.
