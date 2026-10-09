# V34 timing-reset recovery after structure cleanup

PR281 rebased cleanly onto master16cb3384 (issue260/PR263 plus ratchetPR282).
Rebased4bb720f3 rebuild: Gentoo300/300/0; exact-name set1075/1852 equals the
pre-rebase88993558 set, zero gains/losses; the refreshed1074-name ratchet
passes. The structure cleanup is not credited with new exact functions.

## Four controlled full-TU measurements

`tools/v34_timing_reset_reproduce.py --domain docs/v34-timing-reset-domain.md`
uses the complete current Gentoo config, bug reproduction, executed assembler
version and all RTL dumps. Raw unchanged V34RX baseline reproduces production.

| Reset order / point view | Bytes | Verdict |
| --- | ---: | --- |
| retained / packed | 249 | SIZE12 |
| original / packed | 249 | SIZE12 |
| retained / separate IIR taps | 259 | SIZE2 |
| original / separate IIR taps | 259 | SIZE2 |

Original is261B. The selected combined control independently reproduces ALL
23 constant stores in original offset/width/value order, including Q(-2) before
I(-2) and separate 16-bit stores. Existing `dp.iir2` is the timing meaning of
these four bytes; `dp.point` is the alternate decision view. No new type or raw
offset accessor is introduced. The zero final object values remain identical;
there are no calls or conditional reads among these reset stores. Helper call
and burst pointer publication remain first. No unique source syntax is claimed.

`tools/v34_timing_reset_audit.py` fires on the selected sequence and rejects
all three retained-axis negatives. Four full TU controls/48 body verdicts:
metadata, bindings, allocated data/BSS and canonical nontext relocations agree;
text positions move for the half-store cells. Both keep all baseline exacts,
gain none. Order-only changes just the target. Half-store controls change six
bodies: rxtiminginit, rxinit, rxtiming, receiver, V34demodulate, modem_serrint.
The other six bodies remain raw-identical. These five nonexact bystanders are
included, not suppressed: V34demodulate grows12 bytes, modem_serrint grows41;
receiver/rxtiming each have two changed bytes, rxinit120. No unproven register-
only explanation is attached to this collateral or the target's residual.

## Compiler boundary and limits

Stage comparison covers29 stage pairs/58 streams for rxtiminginit; first
source divergence is01.rtl. Observable immediate-store splits change22→23.
Artifact `build/v34-timing-reset/stage-diff.json` retains the differing patterns.
Subsequent read-only installed-compiler replay validates two raw/traced/saved
object triples, 249 searches/1,620 candidate visits. Target searches increase
23 to24 (including its epilogue split); both enter at cursor50. Every enclosing
replacement succeeds. This measures the current code's search effect, not the
original compiler's unobserved history or a unique residual cause.
The remaining two-byte size difference plus allocation/zero materialization
changes do not authorize register fitting, literal/store permutations or a
byte-exact claim. This finite order/view domain is closed. Previous EchoFilter
cursor/count domains remain closed; they were checked before proposing a repeat.

The public VPcm accessors' repeated original obj+4 base is a separate observed
lead. A register base alone does not prove the extent/type of a new nested
region, so no new struct/pointer offset view is fabricated here.

Artifacts: build/v34-timing-reset/results.json, audit.json and stage-diff.json;
build/byteident-post260-baseline.json. Finding F11881; Playbook updated.

Replay the two saved controls with gentoo_peep2_search_reproduce.py using
`--manifest build/v34-timing-reset/trace-manifest.json
--output build/v34-timing-reset/traces`; the combined audit validates both.

## Final validation

Production V34RX complete object raw-identical to the selected full-TU cell.
`make phase J=8`: period388 passed/0 failed and structural boundary green;
`make tc J=8`:300 sources/300 objects/0 failed. Static285 suites/10,038 anchors
are unique/attached. Full final exact-name set equals the rebased baseline:
1,075/1,852 exact,118,176 exact bytes; zero gains/losses. The master-refreshed
1,074-name ratchet passes and is unchanged. No new whole-function exactness or
modern portability claim. The earlier PR281 SDMv27 gain remains preserved.
