# FloatARMA output destination and feedback acquisition

93d7eee1 source, complete retained Gentoo C++ profile. Four cells cross two
object-derived boundaries:

* arma_convolve returns a float (baseline) versus writes through an explicit
  float output pointer. Blob scalar process computes member addresses +0x2c
  and +0x30 before each dot-product loop and stores the finished sums directly
  through those addresses. Ours narrows through a temporary stack float then
  copies it to each member, retaining a loaded first result through the second
  loop. Replace helper return with output pointer in all four call sites.
* Scalar process captures y/ypos at entry versus after forward sum publication.
  Blob loads +0x0c/+0x24 after the first sum, while ours loads them beforehand.
  Array histories cannot alias object fields for a valid owning instance, but
  a direct member output pointer can affect GCC alias/lifetime analysis.

Preserve sum grouping, long-double accumulators, tap-tail semantics, member
widths and order of published results. No function/declaration permutations,
flags, allocator changes or semantic tolerances. If no complete hit, close
these boundaries; size-only improvements are not adopted. Audit seven TU
functions, inline bodies, bindings, data and relocation targets. Runtime gate
belongs to final batch owner, not per cell.

## Result: closed, not adopted

4/4 complete TUs valid; 5/7 exact retained in every cell. Scalar baseline408
vsblob392; late-feedback only reaches392/BYTES275, output-pointer only
392/BYTES316, both392/BYTES152. Block baseline466 vsblob562; either
output-pointer cell482, still80 bytes short. Thus both observable boundaries
influence code generation, but neither identifies the complete source preimage.
No candidate adopted. Exact constructors, reset and destructors unchanged;
all metadata/data/nontext/relocations audits pass. ANSam pool separately is
already4/4 exact, so no source experiment performed there.

The first generator failed before compilation because the shared extractor
expects a function name without its parameter list; log preserved at
build/batch20-arma-output-generator-invalid.log. Corrected by using the exact
scalar overload boundary, then all four cells rerun. Invalid attempt excluded.
