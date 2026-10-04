# Tone IIR section expansion and tap counter width

Four complete93d7eee1 TUs cross two original observable boundaries in both
progress functions. Blob has four expanded sections (recorded as hand-unrolled
since initial reconstruction), whereas source loops s=0..3. Each blob tap loop
increments and sign-extends lowword before cmpw2; source int k loses that
counter-width boundary. Cross short k with four literal section blocks. Keep
sample counter i int, all products/accumulators32-bit modular, history order,
output narrowing and first-three/unconditional-last-positive shift semantics.

Explicit section blocks preserve each existing block and replace only s with
its section constant. No pointer/member reorder, helper injection, flags or
loop spelling search. Inspect full two bodies plus six bystanders; rawbaseline,
profile/bugdefine/actualassembler/RTL and metadata/data/relocs mandatory.
Close this four-cell family if no completehit; gate at batch end for adoption.

## Result: recovered boundaries, incomplete functions

4/4 valid complete TUs retain3/8exact. toneiir_progress692→1029(expansion),
753(shortonly),1126(both) vsblob1141. _iir_filter_progress306→673,315,782
vsblob802. Both axes matter; no fullhit, no adoption. Six bystanders unchanged.
Initial RTL shortk remains promoted reg/v:SI, with two lowword sign-extension
writes per literal section; detector counts0/2/8 as appropriate. Merely reading
physical pseudo mode would wrongly conclude the shortcontrol had no effect.

Separate shiftcursor domain declared from new independent missing pointer
operations; original section/width domain remains closed.
