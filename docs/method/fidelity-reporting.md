# Report function fidelity separately from size

Use **EXACT**, **REGALLOC** and **NONMATCH** to describe function fidelity.
Keep **UNRESOLVED** and **NODATA** separate: incomplete evidence is not a match.
Equal byte length does not establish that any instruction has been reproduced.

- **EXACT:** the existing byte and canonical relocation-target comparison passes.
- **REGALLOC:** the existing instruction/operand comparison passes under its
  consistent register-renaming rules.
- **NONMATCH:** a byte, extent or relocation-target mismatch remains.
- **UNRESOLVED:** the relocation evidence cannot establish a complete comparison.
- **NODATA:** a body could not be read or accepted for comparison.

Anonymous switch tables can supply an EXACT relocation destination through
`tools/toolchain/guarded_switch.py`. This narrowly supports an unprefixed
32-bit EAX `SUB`/`CMP` followed immediately by unsigned `JBE` to an indexed
`JMP disp32(,EAX,4)`. The ELF32 i386 REL proof requires one bounded, allocated,
read-only, non-merge table, complete non-overlapping entry relocations, and
ordered instruction-boundary targets inside the unique owning function.
Named aliases and known named, direct or relocated bypass entries into any
byte of the protected instructions refuse the proof. Unsupported relocation
kinds also refuse it.

The owner decoder accepts an explicit, bounded instruction vocabulary and
checks the byte encodings of supported control transfers. Prefixes and
unmodelled transfers such as `XBEGIN` refuse the owner proof. The scan for
known direct entries also unwraps prefixes and includes `XBEGIN` destinations
outside the sized owner, including its 16-bit relative form.
Far jumps and calls anywhere in the scanned code section refuse the proof;
their segment semantics are outside this decoder's supported domain.

The descriptor retains the owner, dispatch position, entry width and count,
and every ordered function-relative target. `body()` admits it only for the
sole remaining unresolved relocation. EXACT still requires all other masked
body bytes, relocation positions, kinds and canonical destinations to agree.
Instruction-run measurements remain available, but this descriptor explicitly
prohibits REGALLOC admission: it supplies no ABI or register-renaming proof.
An exclusive observed relocation reference establishes the known reference
set in that ELF; arbitrary pointer arithmetic and unrecorded external jumps
remain outside the function-entry boundary. Binding, visibility and final
object identity retain their separate checks.

Run `python3 tools/toolchain/test_guarded_switch.py` for the synthetic ELF
controls. Optional `--actual OBJECT --blob REFERENCE` also checks the actual
`RD_create` pair. This is an apparatus recognition improvement; unchanged
source objects do not constitute a source-recovery gain.

The CLI and CI tables show these outcomes and the EXACT + REGALLOC total.
REGALLOC and NONMATCH counts have no independent preferred direction. For
example, NONMATCH → REGALLOC increases REGALLOC, while REGALLOC → EXACT reduces
it; both improve fidelity. Moving between equal-size and different-size
mismatches is not progress.

## Contiguous instruction runs

Each function reports its longest common **contiguous** instruction run as a
count and percentage of the reference function's instructions, with the
candidate instruction count alongside it. Padding is removed using
`byteident.py`'s existing padding policy. This is a diagnostic for partial
recovery; it neither certifies a match nor excuses a failed differential test
or protected-symbol loss. A complete reference run can still occur inside a
candidate containing extra instructions.

The `operand-run-v1` normalisation in `tools/toolchain/fidelity.py`:

- abstracts general-purpose register identities while preserving widths and
  high/low byte lanes; stack-pointer registers and x87 stack positions remain;
- preserves instruction mnemonics, including conditional branches, constants,
  memory offsets/scales, relocation kinds and canonical symbol targets/addends;
- abstracts local branch byte offsets and section-relative relocation addends,
  which vary with layout.

It does not prove register relationships or branch destinations. These limits
are why a long run is evidence about reproduced instruction structure rather
than an equivalence verdict. This normalisation is stricter than the historical
`structsim.py` canonicalisation; scores from the two tools are not interchangeable.

For multiply-defined functions, the run is measured in **every defining copy**
and the shortest run is reported. An unavailable copy makes the conservative
symbol-level measurement unavailable. This selection is independent of the
existing copy-selection policy used by the proof census. `--why` shows every
copy rather than selecting a lucky one.

## Commands and records

```sh
make byteident
python3 tools/toolchain/byteident.py --near 10
python3 tools/toolchain/byteident.py --why SYMBOL
python3 tools/toolchain/byteident.py --json-out build/function-fidelity.json
```

`make byteident` first refreshes period objects. Direct CLI use still refuses a
stale object tree. The default NONMATCH table and `--near` rank measured
functions by descending contiguous-run coverage, then run length. Byte counts
and signed candidate-minus-reference deltas appear as diagnostics. Unavailable
runs are reported in the denominator and excluded from ranking.

JSON adds `nonmatch`, `function_fidelity` and `instruction_runs`. Each function
has its outcome, selected defining object, defining-copy count, reference and
candidate byte extents, signed byte delta, and instruction counts/run/percentage.
An unavailable measurement has `instructions: null` and a reason. For a rejected
body, byte diagnostics use the declared symbol extent.

The run summary states its version/normalisation, compared/measured/unavailable
function counts, and the sum of per-function longest runs over the measured
reference-instruction denominator. This sum is not one contiguous run across
the object. CI lists changed per-function runs with counts and percentages,
reports listed/total and base-only/head-only counts, and puts byte-size changes
in a separate diagnostic section.

Old JSON remains readable. Its missing run data is **unavailable**, never zero.
New records are validated against their counts, percentages, EXACT symbol set
and legacy protected membership. Cached CI measurements include the new helper
in their cache key, so base and head use the same normalisation.

## Compatibility and validation

Low-level `byteident.verdict()` reasons, legacy JSON fields (`bytes`, `size`,
`reloc`, and others), displacement records and all three ratchet membership sets
remain unchanged. They support existing diagnostics and consumers. In
particular, legacy `grade01` includes UNRESOLVED; the public matched total
explicitly uses EXACT + REGALLOC and does not inherit that accounting.

Fresh PR comparisons protect named EXACT/REGALLOC membership, not just its
count. `period_metrics.py --check` rejects a lost member even when another
function replaces it. REGALLOC to EXACT preserves membership; EXACT to
REGALLOC still requires the separate EXACT-loss review. Legacy reports remain
readable, but a count-only report containing REGALLOC cannot certify this check.

New stored floors also record `matched` and `matched_symbols`. Every named
member must remain EXACT or REGALLOC; UNRESOLVED does not qualify, and a
displacement cannot excuse that loss. Historical floors without these fields
retain their legacy semantics. This protects against classifier changes that
affect both sides of a fresh comparison identically.

Advance a membership bank by union when it includes a historical, body-pinned
recovery target. Retain that target and its displacement rather than replacing
the bank with a current snapshot. Banked target counts can therefore differ
from current measurements; use the current `--json-out` report for the latter.
Do not use `--update` to erase a pinned loss.

On 2026-10-31, the unchanged-object comparison over 1852 symbols preserves **every legacy JSON
field**, including all three protected symbol sets. All 1852 functions have run
measurements, with none unavailable in that measurement. This is a tooling
change, not a source-recovery gain. The measured local headline is 1163 EXACT,
37 REGALLOC, 651 NONMATCH, 1 UNRESOLVED and 0 NODATA; these are a dated validation
result, not permanent counts.

Detector controls cover renaming/padding, changed conditions, immediates, field
widths/offsets/scales, byte lanes, x87 positions, call targets/addends and
relocation kinds. Further controls cover insertion versus contiguous matching,
100% reference coverage with extra candidate instructions, worse/unavailable
defining copies, rejected bodies, and all-copy `--why` output. Report controls
reject inconsistent JSON and distinguish legacy/unavailable data from measured
zero. Existing loss-review and ratchet accept/reject controls remain in place.
