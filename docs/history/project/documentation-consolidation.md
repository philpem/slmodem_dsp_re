# Documentation consolidation review

Baseline: upstream `4169cb3b`, 16 October 2026. The work is isolated on
`docs/manual-consolidation`. Production source, headers, compiler settings,
fixtures and comparison tolerances are unchanged.

## Disposition

The [migration ledger](../migration.tsv) accounts for 722 original root files.
Seven technical references entered the Maintainer Manual; 715 dated plans,
experiments and investigation records entered topic directories under
`docs/history/`. Original copies of the seven references remain under
`docs/history/reference/`. Short pointers preserve 33 original paths used by
source comments and tools. Primary findings, deviations, current objectives,
generated attribution and tool-consumed registers retain their paths.

All 715 investigation records were ingested into Hindsight in 53 bundles,
titled `Documentation history migration (4169cb3b), bundle N of 53`. Each
record carries its original path, archive path and baseline. The seven
original references were ingested separately with historical-status warnings.
The correction document `Correction: historical configuration, training and
sample-rate notes` records the source-verified superseding facts below.
These writes preserve evidence for recall; they do not assert that an old
measurement describes current source or that a knowledge-page refresh has
completed.

The manual's new catalogue groups all 300 production source inputs into
64 runtime modules and explains responsibilities, algorithms and source
ownership. A separate chapter explains V.34 probing, pre-emphasis selection,
complex LMS updates, carrier/timing recovery and error publication.

## Technical documentation review

The editorial pass used `tech-doc-style` and `acorn-style-reviewer`, with
modern defaults and British English. The targets were new/current manual
pages, navigation, documentation indexes and the migration's references.
Historical records were checked for preservation rather than restyled.
This is an authorial checklist review; independent PR review remains a
separate step.

The following findings were corrected. Locations name the affected sections;
the quotations identify the earlier wording preserved in the original studies.

| Location | Rule and category | Finding and correction |
|---|---|---|
| Configuration: station role | C8, error | `is_answer` described configuration `+0x04` as a direction flag read only by mark holding. The header and constructor identify `v21` and its tone-plan reads; the manual now distinguishes role, tone plan and the asymmetric combination. |
| Configuration: transport delay | C8, clarity | “Set 240” presented a historical engineering proposal as general advice. The manual now separates host measurement, constructor arithmetic, no-echo fixture results and deployment validation. |
| Parameters: cadence units | C4, error | Six 666-sample intervals were described as 500 ms. They are 499.5 ms at 8000 samples/s; the example now explains quantisation. |
| Parameters and coefficient references | C8, error | Several routines were still described as unreconstructed. The manual points to their current implementations and keeps old plans in history. |
| Call-progress tables | C8, error | The state-name order remained “not yet proven”. Current `Callprog.c` defines and indexes that table. Conditional detector selection now has one current explanation. |
| Terminology: PPS | C6, clarity | A single expansion mixed receive phase splitting with transmit pulse shaping. The manual identifies both uses and their direction. |
| Training: pre-emphasis | C8, error | The old study described `probe_preemp_fit` and a Theil–Sen fitter. Current source uses `probe_preemp_shape`, eleven templates, a fallback counter and a compatibility flag; the new chapter describes these controls. |
| Training: adaptation | C8, clarity | The presence of two update routines suggested simultaneous competition. The receiver's `DATA` if/else and earlier `TRAINED` bypass are now explicit. |
| Sample rates: wrapper | C8, error | Unsupported unequal rates were said to run silently without conversion. Current construction refuses them before reverse dispatch; the manual points to the existing refusal fixtures. |
| Sample rates: unidentified timer | C8, clarity | An unidentified counter was said to “must move” with known turnaround timers. The manual now requires establishing its role and units first. |
| Coefficients: design interpretation | C2/C8, clarity | A measured fit was described as fully characterised or strictly better, and a transition extending to 4400 Hz was said to stop below 4000 Hz. Wording now preserves the measured band, approximate fit and limits of the performance claim. |
| Coefficients: bank denominator | C8, error | Eighteen mode entries were described as eighteen tables. `FixedRC.c` defines sixteen distinct polyphase coefficient blocks because modes 2/4 and 3/6 share pointers. The manual now distinguishes configurations from blocks. |

Resolved findings: seven errors and five clarity problems; no outstanding
findings or mandatory house-style changes. Verdict: **Conforms** for the
reviewed current guidance. This verdict does not certify every historical
claim or establish standards conformance.

## Validation

`make phase J=12` passed all 388 period differential binaries with zero
failures. Structural/provenance checks passed, including 14,512 resolved
references and all 10,040 mutation anchors across 285 suites. This was an
anchor check, not a new mutation campaign. The final editorial pass also passed the documentation audit and reference
recheck. Production inputs remain unchanged from the full phase run.

The strict MkDocs build passed. Run `python3 tools/ci/docs_audit.py` after
building to reproduce the documentation audit and its printed denominators:

- Each production input appears exactly once in the module catalogue.
- Current repository targets and generated local HTML targets/fragments resolve.
- The missing-target and missing-module negative controls both fire.
- All 715 moved records retain their text after normalising documentation paths
  and Markdown link destinations only.
- The seven original technical references match baseline bytes exactly.

Twelve pre-existing links in findings point to untracked historical `build/`
artifacts. They are counted outside the verified-target denominator; this
consolidation neither reconstructs those artifacts nor claims they are present.
External URLs and links pinned to older revisions are also outside the
current-path check. Modern compiler portability was not rerun for this
documentation change.
