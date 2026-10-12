# Supported partial constellation-mask counter recovery

Current baseline347b5b89 revalidates the four-cell domain originally documented
in [the unsigned clearing-counter experiment](gcc3-mechanism-constellation-domain.md).
The old experiment correctly closed exactness for the counter question; it
declined the measured original branch recovery because no whole body became
exact. Current owner scope explicitly accepts supported partial reconstruction
improvements, so retention is reconsidered without reopening the source domain.

Both original functions compare the clearing counter against7 and useJBE. Signed
int i emitsJLE; unsigned int i emitsJBE. Keep signed which/index k and unsigned
data-loop n unchanged. Counter only takes values0..8; the eight zero writes,
termination, which clamp and all table accesses behave identically for every
valid input. No runtime bug is claimed fixed.

Four current complete-TU cells/44 common verdicts reproduce retained baseline
raw. Each independent counter change modifies exactly one branch in its helper
and one inline branch in setV92CPpckFromParamsInfo. Combined production recovery:

| Function | Before | After | Recovered operation |
|---|---|---|---|
|getConstellationMask|130B/BYTES38|130B/BYTES37|unsigned clearing-loop branch|
|getCodecConstellationMask|130B/BYTES40|130B/BYTES39|unsigned clearing-loop branch|
|setV92CPpckFromParamsInfo|SIZE4|SIZE4|both inline copies of those predicates|

All other instruction/operand rows unchanged in those three bodies. Eight
bystanders unchanged canonically; all symbol metadata, allocated/named data and
nontext relocations unchanged.4/11 exact perTU unchanged, zero exact gains/losses.
This recovers observable operations and counter type class, not uniquely named
original declarations or whole-function identity. No source-order/profile change.

Replay the current four cells then audit; integrated audit additionally requires
production source and complete object raw equality to combined candidate:

```
python3 tools/gcc3_constellation_counter_retention.py --domain docs/constellation-counter-retention-domain.md
python3 tools/gcc3_constellation_counter_retention_audit.py
make tc J=8
python3 tools/gcc3_constellation_counter_retention_audit.py --integrated
make phase J=8
```

Later replay must pass archived347b5b89 objects/config and historical headers
if needed.

Integration completed: production source and object equal combined cell raw;
one of300 objects changes, all299 others and full.build-config unchanged.
make phase J=8 exits0:388 period tests passed/0 failed; all structural checks
green, no detached/nonunique/wrong-arm locators. Whole-tree ratchet passes with
the same exact set1080/1852 and119108 original exact bytes;673 SIZE/64 BYTES/
30 REGALLOC/5 UNRESOLVED. Refcheck14426 references/3027 headings clean.
No mutation/fuzzing execution or modern portability claim.

Faithful300-input partial link retains701452 bytes of.text. Its before/after
text differs at precisely four bytes, each JLE0x7e→JBE0x76, matching the two
helpers and two inline predicates. Strict original-relative comparison remains
DIFFERENT/exit1 both before and after; whole-object identity is not claimed.
Artifacts: build/constellation-counter-integration and byteident-constellation-
counter-final.json; phase log build/constellation-counter-phase.log.
