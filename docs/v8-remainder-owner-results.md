# V8 remainder and captured-decision controls

At38626248, PR270 has landed:1057/1852 exact functions,114842 exact original
bytes. This pass changes no production source/header and does not move that
census. PR263 observed97e8a10c remains a separate V34 structure-cleanup branch.
No #22 writes, runtime, fuzzing or mutation execution.

## What the original distinguishes

Four mark/space drain/flush conversions store the low two run bits before
pushing bits. Original drain rounding reloads this remainder. Retained helpers
return it after pushing; the drain result is then immediately cleared. The
original loops guard zero, decrement after a push and branch on zero. This
differs from a positive or postdecrement source predicate even when normal
counts behave identically. Guarded-do controls recover DEC/JNE with intervening
flag-preserving MOVs; adjacent-instruction matching would miss every original
backedge.

Original coefficient pointers a,b,c,d at0xdd8/ddc/de0/de4 establish energies:
EDX=c²+d²=mark, EBX=a²+b²=space. SUB78e49, TEST78e4b, SETG78e4d capture the
positivity of the wrapped mark-minus-space difference before silence assigns
case2. Dispatch78e6b..7d tests1, unsigned-below0, then2. This is more specific
than the retained direct energy comparison plus nested branches. An unsigned
three-value decision reproduces JB where a signed switch emits JLE.

Four original mark/space bit-input loads use MOVZWL. Unsigned-short helper
formals reproduce those zero extensions; existing bit operations already
convert to unsigned short. None of these witnesses establishes the unique
original spelling or proves a complete reconstructed function.

Wrapped subtraction versus direct signed comparison is not universally
equivalent. Two -32768 squared components produce the INT_MIN energy bit
pattern; paired200/100 components produce50000, outside silence. Wrapped
positivity and direct comparison disagree. These are algebraic internal-value
examples, not demonstrated reachable modem histories or differential fixtures.
The control spells subtraction through unsigned arithmetic then converts to
period32-bit int for the positivity test. No source adoption follows.

## Declared full-TU domains

| Domain | Cells | Actual target sizes, bytes |
| --- | ---: | --- |
| Run publication/countdown |6| baseline1121, unsigned-positive1121, unsigned-zero1083, published-zero1103, unsigned-countdown1045, published-countdown1061 |
| Bit-input extension |5| baseline1121, published-zero signed/unsigned1103/1103, published-countdown signed/unsigned1061/1061 |
| Captured decision |5| baseline1121, published-countdown1061, direct-if1077, wrapped-if1093, wrapped-switch1097 |
| Unsigned decision/bit cross |4| baseline1121, signed-switch1097, unsigned-switch1089, unsigned-switch/zero-bit1089 |

Original is1100B. SIZE(N) reports an absolute length difference, not its sign
or a count of wrong bytes. An initial progress calculation incorrectly added
these gaps to1100; actual nm/ELF sizes above supersede it. Source and full raw
object hashes prove the repeated controls agree. No compilation was invalid.

Twenty cells,80 emitted-body comparisons and80 original-common verdicts;
all four raw baselines reproduce production. Two cross-package source/raw-object
repeats pass. All symbol metadata/binding, allocated data/BSS and canonical
nontext relocations remain equal. Three bystanders stay unchanged; only
v8_fskdemodulate changes. Exact set remains2/4 in this TU:0 gains,0 losses.
Initial RTL owns the wrapped decision pseudo before dispatch; loop and final
dumps retain it. Named-pseudo annotations disappearing after allocation are
not evidence of elimination. Each cell saves all RTL stages and annotated
assembly with actual commands, compiler/selected assembler identity and hashes.

The audit's six witness controls include signed/unsigned dispatch, signed/zero
extension and guarded-countdown/postdecrement refusal. They fire on measured
inputs before trusting any absence. The first detector incorrectly required
adjacent DEC/JNE and failed its original positive; the correction allows only
intervening MOVs that preserve flags.

## Next discriminating work

The original parameter owner starts at0xc2c, whereas the current combined
transmit/receive parameter struct starts0xc20. Original bit/count accesses use
+4/+6/+0xc/+0xe; current access uses+0x10/+0x12/+0x18/+0x1a. Header review
shows six transmit fields followed by receive/framing fields, making a nested
receive subobject plausible. However, base-address selection alone cannot prove
a source structure. Inspect independent initializer/caller pointer formation
and typed extents before defining a subobject. Audit every header consumer if
that evidence supports restructuring; do not fabricate a byte-array alias or
subobject solely to force a register base. This scope is disjoint from PR263.

The tested four domains are closed without adoption. They refute a blanket
claim that every residual here is already pure register allocation, but do not
claim to have solved the original inline profile or the whole function.

## Replay

Run each generator with its matching domain and the archived production
objects/config at38626248 (or --historical-headers for explicit snapshot replay):

```sh
python3 tools/v8_remainder_owner_reproduce.py --domain docs/v8-remainder-owner-domain.md --baseline-dir build/production-before
python3 tools/v8_bit_extension_reproduce.py --domain docs/v8-bit-extension-domain.md --baseline-dir build/production-before
python3 tools/v8_decision_owner_reproduce.py --domain docs/v8-decision-owner-domain.md --baseline-dir build/production-before
python3 tools/v8_unsigned_decision_reproduce.py --domain docs/v8-unsigned-decision-domain.md --baseline-dir build/production-before
python3 tools/v8_remainder_owner_audit.py
```

Artifacts live on ZFS under build/v8-{remainder-owner,bit-extension,decision-owner,
unsigned-decision}/ plus build/v8-remainder-owner-audit.json. No new runtime
fixture or production edit exists, so this tool/findings pass does not rerun
the unchanged period suite or claim new behavioral validation.
