# Scalar template return and write-back boundaries

Baseline9fabd432 on PR #284, master736cf61c unchanged. Fixed Gentoo GCC3.4.2-r2
profile, selected period assembler, DSPLIB_REPRODUCE_BUGS throughout. No
optimization-profile, binding, layout or register-forcing change.

## Three original bodies recovered

| Symbol | Original bytes | Recovery |
| --- | ---: | --- |
| Descrambler<int,int>::process(int) |110| Scalar pTap2 decrement before pTap1 |
| Descrambler<unsigned char,int>::process(unsigned char) |118| Same order plus output-type return |
| Scrambler<unsigned char,int>::process(unsigned char) |118| Scalar pTap2 decrement before pTap1 |

These are three distinct names/346 original bytes. The Scrambler HI function
also reproduces independently in both V90Phase3Modulator and V92Phase3Modulator.
Those two copies count once in the strict symbol census.

The original Descrambler HI method returns full EAX. Its getV90Decision caller
passes that value to bitsToInfo(int) without the retained MOVZBL AL at either
call. Retained scalar source instead returns input type T, explicitly casts
the I intermediate to T, and forces extra narrowing in producer and consumer.
The consistent I return with plain return r restores both boundaries. Byte
history/input XORs remain bounded0..255; no ordinary result-value defect or
unique original type spelling is claimed. The return type is not encoded in
the member's mangled name; caller consumption supplies independent evidence.

The declaration/body cross is informative: body-only is raw-inert, declaration
alone fixes callers while retaining the producer's late cast, both restore the
producer's118-byte length but leave10 differing bytes. Original tap write-back
is pTap2 at+0x18 before pTap1 at+0x14. Crossing that independent scalar source
order with output return closes HI only with both changes. II already returns
int and closes from order alone. Initial RTL stores already change order;
this is not an invented register permutation. The complete native bodies,
including both return arms, calls and relocations, then match exactly.

The adjacent scalar Scrambler transfer is not inferred from analogy. Its own
HI original has the same write-back order, but its already-exact HH original
has the opposite native order. A shared pTap2-first source control makes HI
exact in both consumer TUs while preserving the *entire* V92Phase4Modulator
object raw, including HH. Thus native write-back order alone cannot dictate
shared source: the exact sibling and full-TU controls distinguish scheduling
from source order. No XOR spelling, declaration order, bulk method or
per-instantiation specialization is changed.

## Imported host counts: independent partial recovery

Vendored ref/slmodemd/modem_dp.h:82–83 and modem.c:594/612 agree on int count
for modem_get_bits and modem_put_bits. B103/V23 locally declared unsigned
short; V22/V32/VPCM already use int. The original V23 caller passes full
n_tx/n_rx while retained source explicitly narrows them before these calls.

Eight complete-TU controls cross consistent int imports with cast removal
across B103 and V23. Declaration-only and cast-only controls are raw-inert.
Their combination removes both original-absent V23 extensions and changes
SIZE10→7. Adopt the original full V23 handoff. B103 has independently witnessed
word normalization: removing those casts changes its original argument
interpretation and moves SIZE11→13, so keep its casts and correct only the
declaration. B103's entire production object remains raw-identical. No claim
that huge counts occur in a valid modem lifecycle. A stale explanatory V32
comment is updated; its entire object remains raw-identical too.

## Complete domains and audits

| Domain | Complete-TU cells | Shared function verdicts |
| --- | ---: | ---: |
| Return/body discriminator in Phase4Demodulator |4|80|
| Return/body cross in all27 transitive C++ consumers |108|2164|
| Scalar descrambler order/return cross in both producer TUs |8|184|
| Selected descrambler header in all27 consumers |54|1082|
| Imported count/caller normalization cross |8|40|
| Scalar scrambler transfer and exact HH control |6|188|
| Combined header in all27 consumers |54|1082|
| Total |242|4820|

Every unchanged full-TU baseline reproduces raw. Symbol metadata, named data,
allocated non-executable data, canonical relocation targets, ordered call
targets and every body/bystander are audited, including losing controls.
Changed executable COMDAT sections are code, not silently exempted data.
The only changed bodies are the three recovered scalar names, getV90Decision,
getV92Decision and v23_process. No exact losses. getV90Decision SIZE20→25:
its authentic full return consumption does not make its remaining allocation/
layout closer by size. Do not claim every source correction reduces SIZE.

The primary return study saves RTL for stage tracing; all-consumer controls
save complete compiler commands, versions, headers/hashes and objects without
duplicating every dump. Two script-patch context failures changed no files and
produced no experiment results. All compiler controls completed successfully.

## Validation boundary

The first full gate reported387/1 because the existing t_fuzz fixture selected
the old unsigned-char member-pointer overload. Its run log was stale, not an
executed result; the period compiler rejected the signature. Correct only the
selector's return type to int; no input, assertion, tolerance or denominator
changes. That intermediate full run is not the final validation authority.

Automatic approval review rejected another full make phase because it would
execute t_fuzz, contrary to the user's instruction to skip fuzzing. Final
validation therefore explicitly selects the387 non-fuzz binaries and runs
the other phase structural/provenance targets separately. No request to bypass
the rejection, no fuzz/mutation campaign, no full-phase or modern-portability
claim. Static mutation locators are retargeted to the same operations only.
The static anchor denominator remains285 suites/10038 anchors.

Final strict census is1085/1852 EXACT/120105 original exact bytes, up3/346
with no losses;668 SIZE/65 BYTES/29 REGALLOC/5 UNRESOLVED. Only5/300 canonical
objects change,295 stay raw-identical, all27 selected header-consumer cells
match production raw, and all300 faithful-link inputs raw-match canonical.
Configuration is unchanged. Partial .text702380→702364 and allocated PROGBITS
918388→918366, but positioned equal bytes69540→69392/943398; strict comparison
remains DIFFERENT/exit1. These are function-level gains, not whole-object
identity or a positioned-byte improvement. Final permitted period run exits0:
387 passed/0 failed, complete388-test list minus t_fuzz. All structural phase
targets pass separately, including285 suites/10038 static anchors. F11980
records the checkpoint; build/scalar-boundary-period-result.json records the
observed result and explicit exclusion.

## Replay and artifacts

- Declared domains: descrambler-output-domain.md, modem-bit-count-domain.md,
  scrambler-scalar-order-domain.md.
- Reproducers: gcc3_descrambler_output.py (option --all-consumers),
  gcc3_descrambler_scalar_taps.py (same option), gcc3_modem_bit_count.py,
  gcc3_scrambler_scalar_order.py (same option).
- Shared audit: tools/gcc3_descrambler_boundary_audit.py;
  build/descrambler-boundary-audit.json.
- Archived old27-consumer objects/config:
  build/descrambler-baseline-objects. Later replay uses --historical-headers
  and --baseline-dir pointing to those matching baseline objects. Original
  four-cell/full-consumer return controls live in build/gcc3-descrambler-output
  and build/gcc3-descrambler-output-all.
- Final non-fuzz selection: build/scalar-boundary-permitted-tests.txt.
  It is the complete388-test list minus t_fuzz, with387 explicit names.
- Canonical baseline hashes/config: build/descrambler-before-objects.json,
  build/descrambler-before-config. Final census:
  build/scalar-boundary-census.json.

## Next discriminators

1. Continue the remaining call-window nominations, looking for return-type
   mismatches with both caller and producer evidence. Three more original
   nominated bodies have been investigated here (Descrambler HI scalar,
   getV90Decision and v23_process);32 remain untouched, and unrelated windows
   inside reviewed bodies are not thereby certified.
2. For getV90Decision/getV92Decision, trace the next original/initial-RTL
   divergence with the corrected shared interface fixed. Do not restore the
   unsupported byte return merely because it produces a smaller SIZE score.
3. Keep scalar order/type domains closed. Transfer to another template requires
   its own original witness and an independently exact sibling/full-TU control.
