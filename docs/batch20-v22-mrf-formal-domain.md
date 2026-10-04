# V22 MRF formal/use handoff transfer

Baseline93d7eee1. Parent exclusively allocates include/dsplib/v22_mrf.h and
matching src/pump/v22/v22_mrf.c helper definition; protectedv22fp.h untouched.
Closed audit: ModDataV22sixlocalownerformsF8120; V22MRFinit8index/samplecells
F11597/98; no retry. Independent DemodDataV22 originalcountboundary: blob
MOVZWL argument→EBP→stack+0xc forMRF; retained caller MOVSWL AX→ESI thenstack.
Its tonecall independently requires signedshortcount and remains untouched.

Fourcells×2TUs cross retainedshort/consistentint MRFcountformal (helper
remaining stayssignedshort; explicitcastonlynarrowingatuse) with caller's
retained/omitted MRF(short)countcast. Only one src caller found by fullsrc
search; all14callerfunctions/3helperfunctions anddata/metadata/canonicalrelocs
in scope. Headerformalwidth not uniquely established bycallee16bitconsumption;
int spelling justified only byconsistentoriginalcaller slot plusrawcalleeproof.
No returntype/headerlayout/flags/profile edits, no invalid declarations.
Stop onboundednegative; no index/store/order expansion.

Results: 8 complete-TU cells, zero exact gains or losses. Both single-axis
controls merge the complete caller baseline. Combined boundary reaches full
510-byte DemodDataV22 but still BYTES231; only that body changes. ModDataV22
remains BYTES59. All helper cells raw-merge the entire retained object; filter
still SIZE29 and init SIZE16. Stable symbol metadata, data, allocated nontext,
canonical relocations. No source/header adoption. Domain closed.
