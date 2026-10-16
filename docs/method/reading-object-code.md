# Reading object code into source

Use these techniques while reconstructing a callable slice in
[the recovery workflow](recovering-source.md). The examples concern GCC 3
output for 32-bit x86 ELF. Instruction encodings and calling conventions need
fresh investigation on another target. Keep an observation ledger with the
instruction range, proposed interpretation, confidence and a discriminating
test for each uncertain decision.

## Keep relocations attached to instructions

An immediate zero in a relocatable object can be a placeholder for a pointer
or call target. `objdump -dr` prints its relocation on a separate line, which
an instruction-only filter can discard. Use a view that retains both:

```sh
python3 tools/dis.py ref/slmodemd/dsplibs.o FPM_div
python3 tools/dis.py ref/slmodemd/dsplibs.o FPM_div --plain
```

The wrapper attaches relocations to operands; `--plain` retains alignment
padding too. It also annotates the reversible x87 arithmetic family using an
Intel-syntax disassembly. Raw bytes, instruction addresses and relocation
records remain the primary evidence when the presentation is ambiguous.

For data references, distinguish a named-symbol relocation from a relocation
against a section symbol with an addend. The latter may identify an object
inside the section rather than its beginning. These commands illustrate the
project's resolver and table extractor:

```sh
python3 tools/relocscan.py ref/slmodemd/dsplibs.o --into AGC_DEF
python3 tools/tabdump.py ref/slmodemd/dsplibs.o --sym seg_end --type s16 --count 8
```

Inspect relocation-bearing ranges before interpreting a table as scalars.
`tabdump.py` warns about embedded relocations and rejects ambiguous duplicate
symbol names. This matters for partial links containing several file-static
objects with the same name. A section offset can disambiguate them.

## Recover the call contract before the body

Follow argument uses from the entry point and check callers as well as callees.
On this target, stack slots and incoming registers can both carry arguments;
do not impose a conventional prototype on an internal function before reading
its callers. Check return-register use and all exit paths. A value left in a
register is not sufficient evidence that callers consume a return value.

Demangle C++ names to recover class, method, overload and template-argument
information. Mangling may establish parameter types that usage alone cannot;
ordinary function return types are often absent from the encoded name. Follow
the implicit object pointer, constructor allocation size, embedded objects,
destructor calls and vtable relocations together. Preserve symbol binding and
instantiation ownership as well as the visible function body.

Allocator calls and failure cleanup reveal ownership. For each allocation,
record its size, the stored pointer, the code that initialises it and the code
that frees it. A pointer passed to another constructor may be an embedded
subobject. A destructor freeing a member supports separate ownership. Confirm
the interpretation across the lifecycle rather than from one access.

## Infer width and signedness from operations

Record access width independently of meaning. A two-byte load establishes a
two-byte access; sign extension, zero extension, comparisons and subsequent
uses constrain its interpretation. Signed and unsigned branch conditions
distinguish comparisons that ordinary positive test values cannot separate.
In this project a signedness error remained invisible to differential tests
over the valid small values but was exposed by code generation (finding F613).

Check storage independently from narrowing at a call boundary. Tone IIR's
constructor passes a short argument to a fixed-point helper, but the cadence
producer stores a full 32-bit interval and the progress consumer compares
that full value. A constructor-created interval of 66,036 exposed the old
short-storage model, which incorrectly produced a verdict after 500 samples.
The corrected int member preserves layout while recovering the complete
interval. Audit writers, readers, extensions and owner identity together;
an offset assertion alone cannot validate a member's type.

For fixed-point arithmetic, transcribe a chain of widths and conversions:
input width, multiplication width, accumulator width, shift, narrowing and
store. An arithmetic right shift, division and a cast can differ for negative
values. Record saturation bounds and the order in which they apply. Preserve
intermediate overflow or truncation where the reference exhibits it and the
target language/compiler model supports the recovered spelling.

Use bit operations to infer roles cautiously. One tested bit can justify a
named mask when its meaning is established; a multi-bit field needs a mask,
width and shift. An explicit `and` or `test` does not by itself establish that
the author declared a C bitfield. Compile candidate declarations before
choosing one for reconstruction.

## Build layouts from independent witnesses

Create an offset map for each owner, combining constructor clears, allocation
extent, access widths, array strides, typed callees and destruction. Treat
conflicting witnesses as a reason to investigate rather than silently choosing
one. Reuse one shared definition once the layout is established.

Separate three kinds of uncertainty:

| Evidence | Representation | What still needs work |
|---|---|---|
| Extent known, internal fields unknown | A byte padding span | Field boundaries and types |
| Width or type known, role unknown | A neutral typed field | Semantic meaning |
| Role supported by original text or typed use | A meaningful field name | Further validation of its callers and lifecycle |

Diagnostic format strings are valuable original vocabulary. Follow their
relocations to the exact argument-producing instructions. A nearby string or
source filename does not establish ownership. Typed callee arguments provide
another strong witness; usage inference alone is weaker and should be labelled.

Add size and offset assertions for the period ABI, and prove the assertions
are compiled. This project found guards using a newer predefined macro that
silently disabled layout checks under GCC 3.4.2 (the V3 variance in
[compiler policy](compilers.md)). Debug information generated from your new
types helps navigate mismatches but cannot independently validate those types.

## Recover control flow before choosing source syntax

Identify basic-block boundaries, conditional edges, exits, backedges and
indirect dispatch targets. Follow jump-table relocations and the range check
that admits entries. Include shared tails and out-of-range handling. An
address interval between two case targets may contain code reachable from
several cases, or miss a shared block elsewhere.

Translate the graph into structured C, then compile it with the target
compiler. Several binary loops may come from one source loop after GCC
unswitching; a comparison chain may come from a switch. The V.34 scrambler
and pre-emphasis examples in [refinement](refinement.md) show why source
structure needs a compiler control as well as an assembly reading.

For a state machine, map each dispatch selector to its owning state field.
Trace state writes, counters, event tests, callbacks and common exits. Drive
the machine through real entry and transition calls where possible. If you
plant a state for exploratory coverage, label it and preserve the distinction
from a witnessed lifecycle.

## Preserve loads, stores and callback boundaries

Record when a value is captured, when a pointer is reloaded and when a store
becomes visible. Caching a member across a callback can change behaviour even
if the surrounding arithmetic is unchanged. Distinct pointers may alias at
a valid component boundary, so combining loads or stores needs evidence.

Use focused alias cases when the component contract permits overlap. For
stateful components, also establish the call histories that produce the
relevant objects. A leaf-level alias test establishes agreement for its tested
alias cases; it does not prove that an entire modem connection reaches them.
The scalar-store and reload recovery examples in [refinement](refinement.md)
show how this distinction affects source acceptance.

## Track the x87 stack and narrowing points

Track `ST(0)` through `ST(7)` symbolically, including pushes, pops, exchanges
and the destination of each arithmetic operation. Check subtraction and
division against instruction bytes and Intel syntax when AT&T rendering is
ambiguous. Finding F245 records a reversed divide interpretation caused by
the printed mnemonic.

Record each load/store precision and integer conversion. x87 intermediates
can retain more precision than a stored `float` or `double`; a spill can
therefore change later comparisons as well as final output bits. Check the
control word where rounding or integer conversion depends on it. Distinguish
ordered and unordered floating-point comparisons by their status tests,
including NaN and signed-zero cases where relevant to the boundary.

Do not infer a deliberate source cast or `volatile` store from a spill alone.
Recompile ordinary source with the period compiler first. Register pressure,
inlining and compiler passes may explain the narrowing. A modern compiler
can retain a value that GCC 3 spilled, which is why the period build decides
reconstruction fidelity here (finding F1352).

## Recover tables before generalising their design

Determine a table's extent from symbol size, accesses and neighbouring records;
check those witnesses agree. Extract the exact element representation,
including floating-point bits. For a generator, test every stored element,
the quantisation rule and reachable boundary indices. Preserve embedded
pointers as relocations rather than numeric initialisers.

Then investigate design intent using symmetry, periodicity, impulse/frequency
response, known polynomial forms and the way callers index the data. For
filters, identify phase ordering, tap order, gain normalisation, sample rate
and rounding before claiming a design. Compare all regenerated coefficients;
a visually similar spectrum is insufficient for exact reconstruction. The
[coefficient derivations](../maintainer/coefficients.md) provide worked DSP
examples, while extraction remains useful even when a derivation is unknown.

## Worked example: a fixed-point reciprocal lookup

`FPM_div` combines several of these techniques in a small function. Use the
disassembly command above, then follow these observations before reading the
reconstructed [source](../../src/dsp/fpm_div.c) and
[fixture](../../test/unit/t_fpm_div.c):

1. The entry zero-extends a 16-bit denominator. The zero path returns `1`
   without writing either output; its diagnostic is conditional on debug level.
2. The nonzero path doubles the denominator, retains its low 16 bits and tests
   the sign bit until bit 15 is set. The loop also counts the shifts. A value
   already carrying bit 15 skips the loop.
3. The following arithmetic produces `((mantissa + 0x80) >> 8) - 0x80`.
   The addition takes place after promotion, so it can carry beyond 16 bits.
   The subsequent scaled load reads a 16-bit entry from `FPM_div_table`;
   its relocation establishes which table is used.
4. The function writes the table value and shift count through its output
   pointers, then returns `0`. The calling code uses those two outputs to
   implement division by multiplication and exponent adjustment.
5. Normalisation bounds the mantissa to `0x8000..0xffff`. Substituting those
   bounds into the index expression gives `0..128`, while the original table
   contains 128 entries. Inspecting adjacent reference data explains the
   out-of-range value rather than assuming a missing guard.

For example, denominator `511` becomes `0xff80` after seven shifts and selects
index `128`. The original reads a zero from the neighbouring table. The
reconstruction explicitly supplies that value in the compatibility build;
this preserves the observed result but does not itself reproduce the original
data layout. Functional compatibility and final-object identity remain
separate claims.

The fixture enumerates all 65,536 inputs to the 16-bit entry point, comparing
return, reciprocal and shift, including sentinel outputs on zero input. It
also checks the stored table against both the reference and its generator.
Those checks distinguish a correct transcription from a plausible reciprocal
algorithm that silently repairs the boundary. To run the example:

```sh
make period T=t_fpm_div J=4
```

## Recover diagnostics and defects as observable behaviour

Copy diagnostic text exactly and place calls on the branches that reach them.
Compare transcripts with diagnostics enabled, including argument widths and
ordering. A message's wording does not locate its call site; a missing line
does not alone prove missing signal-processing behaviour. See
[diagnostic evidence](tiers.md).

When an operation looks defective, first check the relocation, width, input
domain and caller history. Record an unmeasured suspicion separately from a
reproduced defect. Measure its trigger and consequences before changing
compatibility policy. This project records deviations separately and gates
intentional fixes behind `DSPLIB_REPRODUCE_BUGS`, so the reference-compatible
build remains testable. Another project should state its own policy before
mixing repairs with reconstruction.
