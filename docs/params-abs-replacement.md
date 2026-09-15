# V90Parameters whole-part absolute-value replacement

This corrects the semantics of `build/params-abs-experiment`: those candidates
added floating `fabs` but accidentally retained the later integer absolute, so
they did not test a replacement. Its member candidate also read
`DIGITAL_POWER_REDUCTION` before assigning the current `pr` and is behaviorally
invalid. That matrix and its store-middle score are diagnostic only and must
not be adopted.

This experiment has exactly two cells: unchanged source and a true replacement.
The replacement changes `int whole = (int)pr` to
`int whole = (int)fabs(pr)`, adds the required `<math.h>`, and removes only:

```cpp
if (whole < 0)
    whole = -whole;
```

The original `DIGITAL_POWER_REDUCTION = pr` statement, fractional declaration,
and fractional integer absolute remain in their original positions. Generated
sources bind over `/src/src/pump/v90/V90Parameters.cpp`, preserving STT_FILE;
the unchanged object must raw-match `build/tc_repro` or the run fails.

The input is `unsigned powerReductionTenths / 5`, cast to `int`, multiplied by
`0.5f`. It is finite, nonnegative, and within signed-int range. Thus floating
absolute before conversion and integer absolute after conversion agree over
the reachable domain; F879 and the existing mutation prove that equivalence.
This experiment concerns source/code-generation recovery only.

Machine results, complete commands, flags, source blocks, hashes, all nine
shared-symbol bodies/relocations, and partial-object contents/symbol records
are in `build/params-abs-replacement/results.json`. Compiler and executed
assembler identity are in `build/params-abs-replacement/toolchain.txt`.

## Result

The two-cell run is valid over all 9 shared symbols. The bind-overlay control
raw-matches the retained object at SHA-256
`f86bf92de5a451015d1f0640f8a6bd175bdb69e34c98d6c7c29ec6fa29b2aa25`:
20,596/20,596 positioned content bytes, 626/626 relocations, and 10/10 symbol
records match. It retains the baseline 7/9 exact symbols, with the target at
`SIZE 12` (344 reference bytes, 332 candidate bytes).

The true `fabs` replacement produces a 344-byte target, exactly the reference
size, and changes its verdict to `BYTES 159`. It retains 7/9 exact symbols and
changes only `loadModemParamsData`; there are no exact gains or losses elsewhere.
Non-text contents remain equal and relocation count remains 626. Offset-bearing
relocation and symbol records after the changed body move as expected; the
target's global binding is preserved and its symbol size changes from 332 to
344. The candidate is therefore an evidence-backed source direction and exact
structural-size recovery, not an instruction-exact closure.

Source SHA-256 is
`96b6131df4d71df3324ce0e80a5881846a5bf101149805aafeb5a71c2378553d`;
candidate source SHA-256 is
`90544c483978f73709b210971a4da4fcf4c5eb89206b21c7f7dc9f5e19e95946`;
candidate object SHA-256 is
`7eb2bb77d15b27ada5c4bd2c7cce03e457b135e3c84caef76f180f43dfc8c829`.
