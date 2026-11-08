# Duplicate-function census and selected linked bodies

The conservative `byteident.py` census and a linked-body comparison answer
separate questions. Preserve the conservative census and its EXACT-symbol
ratchet. Use `tools/toolchain/linkedcopies.py` to inspect every repeated
function name beside the body present in a supplied partial-link artifact.
A linked EXACT result does not promote the conservative grade.

## Reproduce the diagnostic

Build a complete Gentoo GCC 3.4.2-r2 object manifest and recover its input order
with the existing `recoverorder.py` policy. Link those inputs with the Gentoo
image's binutils. Preserve the command, image identity, original manifest,
ordered manifest, order report and linker map alongside the resulting object.
Then run:

```sh
python3 tools/toolchain/linkedcopies.py \
  --manifest build/tc_out/tc_link_manifest.txt \
  --linked build/partial/dsplibs.o \
  --json-out build/partial/duplicate-functions.json
```

The manifest must contain every `.o` in its object directory. Use
`--object-dir DIRECTORY` when the ordered manifest is stored separately.
The tool refuses missing, omitted and repeated inputs. It records hashes of
the reference, manifest, linked artifact and every input object. Those hashes
identify the files inspected; they do not independently establish the build
command or compiler flags.

## Interpretation

The inventory includes every defined `STT_FUNC` record, including local names,
zero-sized symbols and names absent from the reference. It reports repeated
names and their complete definition-record denominator. Local-name collisions
remain separate from external duplicates; a repeated name alone does not
establish COMDAT ownership. Each input record includes its binding, visibility,
section identity and GNU-linkonce/group marker.

For a name with one reference function record, the conservative result follows
`byteident.py`: rank raw per-copy verdicts, choose the worst, break ties by
sorted object filename, then check register renaming on that chosen copy.
The linked result uses the same body comparator independently. Ambiguous or
unsupported linked ownership remains `NODATA`; unresolved relocations retain
`UNRESOLVED`. Neither becomes EXACT.

`linked_body_matching_inputs` lists input bodies whose bytes and canonical
relocations equal the observed linked body. Several matches do not identify
the supplier. No match can reflect a transformation performed during linking
or a mismatched artifact. The tool therefore never asserts the original input
supplier, even when only one body matches. In particular, its manifest is the
*declared* order; an independently preserved link command and map are needed
to establish that an artifact was produced from that order.

Recovered order itself is bounded evidence. `recoverorder.py` explicitly calls
its filename and attribution choices ordering candidates. Its fallback retains
source order. A result under that policy establishes the selected body for
that reconstructed build; it does not recover the original author’s exact
input order or original TU contents.

## Controls

Run the small apparatus controls with Gentoo binutils only:

```sh
python3 tools/toolchain/linkedcopies_selftest.py \
  --work-dir scratch/comdat/controls
```

The fixture uses a weak GNU-linkonce function with two different immediate
values. Reversing input order changes the linked body from EXACT to BYTES
while the conservative result stays BYTES. A repeated local name remains
NODATA at the linked boundary. Separate controls exercise the matching-input
sets, definition denominators, moved relocation addends, omitted objects and repeated
manifest entries.
These controls validate the diagnostic; they do not validate modem behaviour.

## Measurement

Measured at `60804df5b37956fc20fccfa4dfb9ded282fd9711`, using the parent's
frozen 300-object Gentoo baseline. The inventory contains 1,915 function
records, 32 repeated names and 66 repeated definition records. All repeated
names are weak external definitions with GNU-linkonce sections; there are no
local-name collisions in this baseline. All 32 have a unique reference body.
The historical comment in `byteident.py` mentioning 34 multiply defined names
is not the current denominator.

| View over the 32 repeated names | EXACT | REGALLOC | BYTES | SIZE |
|---|---:|---:|---:|---:|
| Conservative per-copy census | 26 | 2 | 2 | 2 |
| Observed recovered-order link | 29 | 0 | 1 | 2 |
| Observed reversed-order link | 26 | 2 | 2 | 2 |

The recovered policy supplies 279 ordering candidates and retains source
order for 21 inputs. The actual link used all 300 inputs. Independent linker
map checks cover all 32 selected symbols in each order: every selected input
is the first defining copy in that manifest. Each linked body matches at
least one input copy under the relocation-aware body comparator. All 300
shared object hashes remain unchanged after the investigation.

| Order-sensitive body | Conservative | Recovered-order link | Reversed link |
|---|---|---|---|
| `Scrambler<unsigned char, unsigned char>::reset` | REGALLOC | EXACT | REGALLOC |
| `Scrambler<unsigned char, unsigned char>` constructor | BYTES | EXACT | BYTES |
| `Scrambler<unsigned char, int>::reset` | REGALLOC | EXACT | REGALLOC |
| `Scrambler<unsigned char, int>` constructor | BYTES | BYTES | BYTES |

The final constructor is an additional order-sensitive body that a list of
grade disagreements alone misses. Its recovered-order copy differs from the
reference at 22 byte positions; the reversed-order copy differs at 15. The
two linked copies differ at ten positions. Neither copy is EXACT, so an
input-order change does not close that reconstruction discrepancy.

The other 28 repeated names retain the same compared body under reversal.
This audit includes the window and arithmetic templates, Descrambler helpers,
serial differential encoder/decoder and every repeated Scrambler method.
The two nonexact window bodies, `hanning<float>` and `blackman<float>`, remain
SIZE in both views and both orders. The selected-view result therefore
removes three *observed linked-body* blockers while retaining their
conservative per-copy reconstruction work.

The apparatus controls report **11 passed, 0 failed**. A third supplied ELF
contains an immediate value absent from both manifest inputs; it remains
BYTES, has no matching input and carries no supplier proof. The controls
also include a
REL-addend movement case: linking moves the section-relative addend while
preserving its canonical destination. Matching input bodies must use the
relocation-aware comparator rather than equality of the raw body tuples.
The assembler and linker both report Gentoo binutils `2.15.92.0.2 20040927`.
No reconstructed source, compiler flag or existing ratchet was changed.

## Proposed playbook additions

Before assigning reconstruction work to a disagreeing COMDAT symbol, retain
its per-copy grades and inspect an independently linked recovered-order body.
Record the input-order evidence and compare a reversed-order control. This
can distinguish an observed selection issue from a defect shared by every
copy, without changing the conservative ratchet.

Keep three closure claims separate: every emitted copy matches, the observed
selected linked body matches, and the complete partial link matches. The last
claim still requires `partialcmp.py`'s strict semantic and raw-file gates.
Neither of the first two establishes source provenance, whole-object binding
identity or behavioural equivalence.
