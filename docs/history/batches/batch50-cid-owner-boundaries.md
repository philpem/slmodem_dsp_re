# CID digit owner and output address domain

Baseline902f47fa, full src/service/cidcore/cid.c TU. Blob cid_get_strings loads the DTMF child once before its 16-byte copy, whereas the retained source reloads it every iteration after a potentially aliasing output store. Blob stores use owner+8 indexed addresses while retaining a separate return pointer; source uses its output local for stores.

Declare four cells before compilation: retained; capture child once on the DTMF arm; spell copy stores as ctx->strings[i]; combine. Preserve mode predicates, loop width/bounds, memset, FSK calls, allocation/type/binding. No padding, flags or artificial locals. Require raw full-TU baseline replay, complete metadata/allocated data/relocation audit and no exact losses. Close domain after four controls unless a new independent instruction witness appears.

Initial run used an incorrect apparatus-only child type (`struct dtmf`); two controls compiled but capture cells failed. Entire run archived as invalid-owner-type and excluded; corrected existing struct dtmf_rx, all four controls rerun.

Valid four-cell result:6/10 retained exacts unchanged. Direct owner stores close145B to BYTES9 with or without explicit child capture; local output store forms remainSIZE1. Complete-TU audit passes allfour; no source adopted. Close this family.

New independent callback lifetime witness: blob ESI is first zero memset value and becomes the return buffer only immediately beforecall, while retained source makes its output local livebefore the call setup. Test fourcontrols crossing post-memset output local initialization with direct owner-relative copy stores (already measured independently); capturechild axis is omitted because its code effect was inert on owner-store seed. Retained raw baseline required; allcounts/predicates/calls unchanged.

Late local+directowner EXACT145B and6/10→7/10, no losses; latealoneSIZE1/directaloneBYTES9. Fullroot74TUauditpreservesallnine neighbours anddata/metadata/relocs. Adopt minimal late/directcell; periodgatepending.
