# PSD conversion-register attribution

Pin5cd55f7a, unchanged complete PSD TU/current headers, fixed Gentoo profile,
mandatory bug define. Production process635B/BYTES6, alpha-equivalent to blob;
six XOR/MOV/PUSH rows in two unsigned-to-float conversion sequences differ.

First prepare full RTL/UID-annotated assembly (-da -dAP) and require complete
object raw equality to production. Trace actual SETs/UIDs for these two sequences
across expansion, allocation/reload, peephole2, register renaming and scheduling.
Then use existing read-only installed compiler observation on the complete
preprocessed TU and require plain/saved/observed assembly/object equality.
Read original GCC sources for the witnessed mechanism. No writes to compiler
state, forced registers, source/flag fitting or arbitrary history permutation.

Prediction: the conversion's materialized high-word zero comes from immediate
push splitting, while the low-word register may be allocation/renaming. Treat
them separately. Falsifier: original-attributed rows have other definitions,
observer perturbs output, or zero exists before the nominated pass. Saved
transformations attribute retained compiler events, not unavailable original
RTL or original scratch state.

Only independently supported source/profile/TU evidence can authorize later
controls. Stop closed scalar/declaration domains. No fuzz/mutation execution.
Apparatus must demonstrate a known event and report all requests/denominators.

Conclusion and executable audit: [results](psd-conversion-results.md).
