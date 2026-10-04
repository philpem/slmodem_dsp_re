# V34 detector initialization boundaries

Local pre-compile declaration, master 803138ff. Findings F11702-F11703 reserved;
PR245 source/header paths excluded. Issue #22 checked before selecting this TU.

The first attempted issue publication was rejected by automatic approval review.
The attempted shell therefore never created its local body file. Four compiler
cells inadvertently ran with a nonexistent declaration path; their artifacts
are preserved in build/gcc3-v34-detector-boundaries-invalid-domain and excluded
from interpretation. This complete declaration precedes the valid rerun.

## Declared domain

The reference detectorinit is186 bytes, baseline172. Its nested counters have
MOVSWL after increments and word comparisons; current section/tap are int.
Reference final emitted stores: polarity,count,state,armed,limit,coeff,
thresh_lo,thresh_hi,level. Baseline source: coeff,polarity,armed,count,limit,
state,thresh_hi,thresh_lo,level.

Four complete-TU cells: baseline; signed-short section/tap; observed tail
assignment order; both changes. Prediction: short counters restore word loop
operations. The tail-order control discriminates source scheduling from a
remaining allocation difference. Falsifiers: unchanged counter-width RTL,
bystander body changes, metadata/data drift, non-reproduction of retained raw
baseline. An exact candidate supplies a source preimage, not uniqueness.
No further domain until the first differing pass boundary is inspected.

Use Gentoo GCC3.4.2-r2 with complete retained .build-config flags, actual
assembler execution, mandatory DSPLIB_REPRODUCE_BUGS appended last, -da dumps.
Validate raw full-TU baseline, exports, data, canonical relocation targets,
all bystander bodies, and exact gains/losses. Adoption requires make phase.
No fuzzing or mutation execution.

V34GiveProbeResults remains closed: direct-double/short-counter gives71 bytes
but lacks independent typed-owner evidence for the reference's owner+4 guard
base. No fabricated header adjustment is tested.

## First domain result and second declaration

Valid four-cell rerun: baseline SIZE14, short counters BYTES18, observed tail
order SIZE14, both BYTES8. Only detectorinit changes; no exact sibling exists
in this two-function TU. Short counters reproduce the complete reference head
and nested loops. Literal emitted-tail source order moves the coeff argument's
load into EBP instead of ECX; therefore machine order is not source order.
The original coeff-first source retains the reference's coeff/limit load roles.

Second finite domain, declared before compilation: keep coeff first, polarity
second, count third. Fully enumerate the six source permutations of
{state,armed,limit}, crossed with the two {thresh_hi,thresh_lo} orders, with
level last and both counters short. Include the original production baseline
and short-counter-only control:14 cells total. Predict the thresholds' reversed
source order fixes their reversed reload roles; one middle-group order may
reproduce the reference's state-before-armed while retaining coeff/limit load
roles. Falsifiers: threshold reversal not changing the role boundary, a
coefficient-load role change in every permutation, any sibling/data/binding
changes, or no exact cell. If all twelve miss, this family closes and requires
a new pass-level mechanism; no declaration/register-name fits follow.

## Measured result

The second domain has one exact cell: state-armed-limit-lo-hi. The adopted
source order is coeff,polarity,count,state,armed,limit,thresh_lo,thresh_hi,level,
with signed-short section/tap. All histories and every final field value are
unchanged. Exactly one of twelve permutations maps in this domain; that does
not identify a unique original spelling outside the tested domain.

The initial/combine dumps retain coeff's first store even though final scheduling
places it after limit. Both counter extensions already exist at combine UID50
and UID62, before allocation. Threshold order changes the later reload roles;
no fabricated register variable, flags, inline-budget change or padding is used.
The literal instruction-order source control fails because it moves coeff's
load into EBP, whereas the matching candidate preserves the reference's ECX
load and EBP limit reload. Source order and scheduled instruction order are
different constraints, and should be crossed with the independently proven
counter width.

Valid denominator:18 cells,16 source hashes,15 raw objects. Four earlier
metadata-invalid attempts remain explicitly excluded. The auditor checks all18
complete TUs:2 functions,0 named data, identical types/binding/visibility,
empty allocated nontext and relocation sections, no tone_detect body changes,
zero exact losses. The sole exact cell matches all186 reference function bytes.
Eighteen selected stage records parse; five positive/negative counter-extension
and store-order controls pass.

A fresh300-object baseline raw-matches merged master's932-exact census. After
adoption299/300 objects are unchanged; detector.c.o raw-matches the exact
candidate. Whole-tree exact set932→933/1852;96167 exact bytes (+186); sole gain
detectorinit, zero losses. The partial-link comparison must remain DIFFERENT:
per-function identity does not establish whole-object layout or original flags.

Replay from the merged ancestry, preserving the original production objects:

```
python3 tools/gcc3_v34_detector_reproduce.py --domain docs/v34-detector-boundaries.md --baseline-dir BASELINE
python3 tools/gcc3_v34_detector_reproduce.py --order-cross --domain docs/v34-detector-boundaries.md --baseline-dir BASELINE
python3 tools/gcc3_v34_detector_audit.py
```

The shared driver uses source revision803138ff, complete baseline .build-config,
mandatory bug define, selected compiler and executed assembler identity. Artifact
ledgers live in build/gcc3-v34-detector-boundaries{,-order}/results.json; audit
and selected stage patterns in build/detector-{full-tu-audit,stage-patterns}.json.

Verification: make phase J=4 passes388/0 with all structural checks. The fixed
detector fixture reports195004 presence checks,397802 absence checks,11980802
warm-up checks,7 behavior assertions and5 coverage assertions. These are
existing fixtures; no new planted state or input-range claim is introduced.
Full refcheck validates14279 references and2807 finding headings.

With a single recovered300-input order reused on both sides, positioned equal
bytes68923→68680/943398; exact relocation records1028→1026/18317; symbol
records394/2907 unchanged. Both complete verdicts DIFFERENT. The16-byte TU
length increase shifts downstream code; the per-function gain does not imply
monotonic improvement of positional whole-object metrics. Unrecovered ordering
controls also remain DIFFERENT and are preserved in separate artifacts.

Static anchorcheck:285 suite declarations,10038 anchors,0 skipped,0 detached/
nonunique,0 vacuous and0 owner mismatches; no anchor edits or harness execution.
