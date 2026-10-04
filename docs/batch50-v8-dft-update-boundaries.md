# V8 DFT update cursor, original table lookup and input-read boundary

Declared eight-cell complete V8Dftc.c cross on902f47fa: sample pointer
advance instead of indexed j; direct byte-wrapped cosine table indexing
instead of inline byte-argument helper; input sample read after phase update
instead of before. Blob advances samples2 at outer latch, shifts/adds/masks
lookup indices in full registers (not low-byte addition), and reads sample
at0x78b5c after phase store0x78b46. Early read can differ on permitted short
aliasing with bin phase; late read is an independently observable boundary,
not a register fitting reorder. Bin counter/phase math/bytewrap unchanged;
no signature or header changes. All eight tuples declared, no synonyms on
miss. Rawbaseline and full actual compiler profile/bugdefine/assembler,
initial RTL and complete data/nontext/symbol/reloc/bystander audits. No
harness/mutation/fuzz; parent batch finalphase.

Result: eight valid TUs retain2/3exact.150/152/172/174B vs164; no gain/loss, only dftupdate changes. Late-read source alone is canonical-body inert; no adoption or semantic gain claimed.
