# Parallel encoder read-order follow-up

## Question

The first corrected matrix's closest form was the 54-byte all-cursor local-result body, BYTES 4. Its only observed instruction-order difference from the reference was the load/XOR sequence: the reference loads state, increments the counter, XORs input, then increments input; the candidate loads input, increments the counter and input, then XORs state.

This bounded follow-up tests whether staged source reads control that ordering. It does not swap operands in one commutative XOR expression.

## Domain and setup

Three retained-profile header overlays were declared:

- the prior all-cursor local-result control;
- state-first staging: `T x = *state; x ^= *in++;`;
- input-first staging: `T x = *in++; x ^= *state;`.

All retain `*out++ = x; *state++ = x;`, reload `size_` in the loop condition, preserve aliasing semantics, and add no attributes or alias promises.

Artifacts are in `build/spectral-encoder-read-order/`. `results.json` records complete commands, all 15 shared function verdicts, changed-body inventories, relocations, symbol records, and data checks. The run uses the published Gentoo GCC 3.4.2 image, explicit Gentoo compiler path, native image user, baseline C++ flags, and `-DDSPLIB_REPRODUCE_BUGS` appended last.

## Result

All three cells compile and are emission-equivalent:

| Cell | Helper result | Exact set | Changed versus retained baseline |
| --- | --- | --- | --- |
| control | BYTES 4, 54 bytes | 10/15 | helper only |
| state-first | BYTES 4, 54 bytes | 10/15 | helper only |
| input-first | BYTES 4, 54 bytes | 10/15 | helper only |

There are zero exact gains and zero losses. All non-text allocated data remains byte-identical, including the 532 named table bytes. Bindings and symbol records remain unchanged, and the helper has no relocations.

## Conclusion

Staging the reads does not control GCC 3.4.2's remaining load/XOR order in this TU. The three-cell domain is closed with no exact hit and no further nearby variants.

The reference still independently supports the broader cached-state, pointer-cursor, local-result, output-before-state source family established by the first matrix. This negative result says only that the two staged spellings do not distinguish the last four bytes; it does not reject that evidence-backed family solely because byte identity remains incomplete. Any retention decision belongs to the parent together with shared-header consumer scope, differential alias coverage, and full-tree gates.

No versioned source, gates, or commits were changed.
