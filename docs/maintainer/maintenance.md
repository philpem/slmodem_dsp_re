# Maintaining the reconstruction

Make each change answer a bounded question: which behaviour or layout is
being recovered, what evidence supports it, and which observation could
disprove it? A plausible modem implementation is insufficient when the
original object is the compatibility target.

## Establish the boundary first

Read the relevant constructor, reset, processing method and caller before
building a fixture. Record whether the fixture represents a public call,
a valid component lifecycle, or a synthetic internal-state probe. Values
planted directly into fields do not establish that a call can reach them.

When comparing reference and reconstructed calls, give each side its own
copies of every writable buffer. For raw state comparisons use
`diff_eq_obj`, which reports differing byte runs and counts an object as one
check. Use `tools/whichfield.py` to translate an offset into the owning
layout. Skip or compare pointer fields appropriately when the two objects
necessarily contain different allocation addresses.

The
[fixture audit](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/p4d-period-fixture-audit.md)
illustrates why an impossible planted history must not drive source changes.
For a new analysis tool, show it detecting a known discrepancy before trusting
a clean report. Report its denominator and failures even when it finds none.

## Preserve the layout's evidence

| Spelling | What it establishes | Maintenance action |
|---|---|---|
| `pad_*`, `unmapped_*` | Unmodelled bytes | Audit accesses; do not assume unused memory |
| `short_358c`, `int_0038` | A recovered shape without a supported semantic name | Preserve width, signedness and derivation |
| Bare `fNNNN` | An offset-based name | Investigate meaning with writers and readers |
| Semantic field/flag name | A claimed purpose | Require supporting evidence before propagating it |

The binary's own diagnostic text is strong naming evidence, followed by
callee/caller types and then usage inference. Document inference as inference.
Where a field is a bit set, use named bit-value constants once its meaning
is supported. Do not replace masks with C bitfields without instruction-level
evidence for that representation.

Define a type in one home. Forward declarations avoid unnecessary header
coupling; duplicate complete definitions can silently disagree on size or
layout across translation units. `tools/onedef.py` checks the current known
debt and rejects new duplicates. Layout assertions need to be active under
the period compiler: `tools/assertlive.py` checks the guard and build support.

## Run the deciding gate

The reference compiler is the Gentoo-patched GCC 3.4.2-r2 identified by the
blob. The repository consumes its Docker image. With Docker available and
the host tools installed:

```sh
python3 -m pip install -r tools/ci/requirements.txt
make tc
make one T=t_vpcmrun
nohup make phase J=$(nproc) > /tmp/dsplibs-phase.log 2>&1 &
gate_pid=$!
wait "$gate_pid"
```

The focused test is an example; choose the test owning your boundary.
Run `wait` in the same shell that started the background gate and preserve
its exit status. Inspect the log separately. A final `tail` or `echo` must not
replace the gate's status. The period summary reports passed and failed
test binaries; check that new binaries are included.

`make phase` includes period differential and structural/provenance checks.
`make portability` explicitly adds modern GCC, 64-bit, interop and coverage
checks; `make phase-full` requests both. A period mismatch is a hard failure.
Do not change `src/` to satisfy modern code generation. Compiler compatibility
flags and shims belong in the build/toolchain apparatus, and a modern failure
needs its own reproducible follow-up. See
[compiler policy](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/method/compilers.md).

When improving test coverage, demonstrate that a meaningful perturbation is
detected. Maintain affected mutation anchors and report uncaught/unusable
cases. A stale mutation snapshot is historical evidence, not a fresh result.

### Example: detecting a scheduling regression

The V.34 datapump's loop-order test exercises the receive-shell latch described
in [per-block supervision](signal-processing.md#per-block-supervision).
The decoder history is built through real initialisation and processing calls;
the final combined receiver-queue boundary is labelled synthetic. A neighbouring
non-crossing case checks that the test responds to the latch transition.
Reversing the two data loops must fail its training-counter oracle and state
comparison. Merely showing both loops run would miss that dependency.

Run the focused period test and measure the complete owning mutation suite:

```sh
make period T=t_v34datapump J=4
python3 tools/mutsnap.py --update --jobs 4 v34datapump
python3 tools/mutsnap.py --check
```

`--update` runs the mutations before recording their verdicts and dependency
key. Finish source, fixture and mutation edits first; never refresh keys by
hand. A current result for this suite does not refresh other suites. The
[coverage record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/maintainer-offset-access.md#loop-order-coverage)
keeps the measured denominators and the distinction between component,
synthetic and public-call evidence.

## Measure the claim you are making

| Claim | Measurement | Limit |
|---|---|---|
| Every service has source | `make coverage`, then `python3 tools/service.py` and `python3 tools/worklist.py` | Requires a populated object tree; says nothing about all paths being tested |
| Exercised behaviour agrees with the blob | `make period` or the period tier in `make phase` | Fixture scope and observables bound the claim |
| Same instruction sequence | `make similarity` | Mnemonics alone do not prove bytes, operands or relocations |
| Function bytes and relocations agree | Period `tools/toolchain/byteident.py` workflow and its ratchet | Function-level identity is separate from full-object layout |
| Entire replacement object agrees | Strict partial-link comparison below | Requires all compared dimensions to be exact |

The final-object completion command is:

```sh
make partial-compare-exact
# Or compare an already-built candidate:
python3 tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
    build/partial/dsplibs.o --require-exact --require-raw-exact
```

`make partial-compare` is a diagnostic census while convergence is incomplete.
A non-failing diagnostic report is not the strict completion gate. Preserve
sections, data, relocation targets/addends, defined and undefined symbol
records and ordering as well as function bodies. `--require-exact` checks the
link-relevant ELF model; `--require-raw-exact` additionally requires literal
file-byte identity, including headers, non-allocated sections and packing.
The convenience target requires both. A `.comment`-only change may pass the
semantic check while failing the raw check.

## Record an investigation so it can be resumed

Before source or flag experiments, read
[experiment design](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/method/experiment-design.md).
Record the base revision, full compiler command, compiler and assembler
identity, input boundary, baseline controls and next discriminating test.
Enable `DSPLIB_REPRODUCE_BUGS` in every reconstruction experiment. Review the
whole translation unit, including collateral non-exact bodies and exports.

Record a suspected original defect in the
[deviation register](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/deviations.md)
with its evidence and reachability status. Record an incomplete layout or
representation issue in the [debt register](reconstruction-debt.md). These
categories need different repairs: naming uncertainty does not establish a
functional defect, and preserving an original bug can be required for fidelity.

For documentation edits, run `python -m mkdocs build --strict` using the
documented environment and check source links against the changed revision.
Keep standards claims attributed, and do not convert an old finding's
measurement into an undated project-wide guarantee.
