# Equivalence evidence: triage, testing, and proof

`tools/eqproof.py` is an **evidence-navigation tool, not a proof engine or a
gate**. Its name is retained from the archive reviewed for [issue
#14](https://github.com/philpem/slmodem_dsp_re/issues/14). A register-renaming
match, a compiled reference, and a passing differential test answer different
questions. Combining them does not turn a sampled result into a universal one.

## What each observation establishes

| observation | narrow interpretation | what it does not establish |
|---|---|---|
| `EXACT` | the function bodies match under `byteident.py`'s byte/relocation comparison | equality of the complete linked object, data, callees, or execution environment |
| `REGALLOC` (grade 1) | the instruction streams pass the linear register-renaming comparison | semantic equivalence under every path, ABI interaction, or input |
| `UNRESOLVED` | a relocation comparison could not be settled | byte identity; this is never credited as `EXACT` |
| `DIRECT` | a compiled test object has an undefined `ref_SYMBOL` reference | execution of that reference, comparison at the symbol's boundary, or a passing test |
| `COMPOSITE` | the named-relocation graph has a potential path from a DIRECT root | a runtime call path or a comparison of the callee's effects |
| `NONE` | that static model found neither kind of reference | absence of testing or runtime reachability |
| passing differential run | agreement on the driven cases and compared observables under the recorded compiler | agreement on untested states, omitted outputs, or other environments |
| caught mutation | the fixture distinguishes that named alternative | completeness of the fixture or equivalence of every surviving mutation |

The relocation graph is incomplete in **both directions** as a model of
execution: taking an address can create an edge that never executes, while
resolved local calls, section-target relocations and indirect dispatch can
escape it. Thus neither a positive nor a negative classification is a coverage
verdict. Compiled references avoid counting unused declarations, but they still
require inspection of the test's calls and assertions.

Exhaustiveness requires an explicit domain, legal initial state, observable
surface, environment, and enumeration or proof covering them. A check count is
not an input count. Checks are collected per **binary**, so attributing its
whole total to each referenced function would inflate per-symbol evidence.
Even a scalar-only signature can depend on global state or floating-point
control state. A pointer can be output-only. Signature shape alone cannot
classify exhaustiveness.

Grade 1 needs particular care. The archived audit tested return-register,
save-class, call-live and loop-rebinding hazards, then promoted a clean census
to a proof. Those probes do not establish a complete semantic model. A replay
of the same binding algorithm is an internal consistency check, not an
independent oracle; a return type from reconstructed headers is an assumption
to justify, not binary evidence. ABI live-ins/outs, implicit operands, memory
effects, control-flow joins, loops, calls and x87 state all belong in an actual
equivalence argument. Current `--grade1` selects a triage population only; it
does not run the archived hazard audit or certify that population.

## Current tools and reproducible census

Use `tools/eqtriage.py` to inspect **the shape of code differences**. Use
`tools/eqproof.py` to connect code-comparison buckets with **compiled test
references and recorded binary outcomes**. The former already supplies
register/operand/scheduling/shape triage; the latter adds evidence provenance.
Neither supersedes `byteident.py` or the project gates. In particular,
`eqtriage.py`'s historical prose about a broken relocation normalizer is not a
current defect report: its self-test now reports relocation markers from both
normalizers. Read the actual control output before repeating that history.

After rebuilding with the default Gentoo GCC 3.4.2-r2 toolchain, collect a
compiler-specific packet and then query it:

```sh
make tc TC_EXTRA=-DDSPLIB_REPRODUCE_BUGS
make capture
make period
sh tools/eqproof-checks.sh build/period --layout period
python3 tools/eqproof.py build/period --layout period
python3 tools/eqproof.py build/period --layout period --grade1
python3 tools/eqproof.py build/period --layout period --class DIRECT
python3 tools/eqproof.py build/period --layout period --class COMPOSITE
python3 tools/eqproof.py build/period --layout period --class NONE
python3 tools/eqproof.py build/period --layout period --sym fComputeRMSValueShortBuf
```

For an already-built modern differential tree:

```sh
sh tools/eqproof-checks.sh build --layout modern
python3 tools/eqproof.py build --layout modern
```

`make capture` supplies the optional SpanDSP replay fixture; without it the
replay binary exits successfully with zero checks, which this collector does
not count as a successful measurement. Report denominators and revision with
each run; the dated measurement below is a snapshot, not a live baseline.
Period differential results decide reconstruction fidelity. Modern compiler
failures remain failures in this collector, including declared divergences;
consult `tools/gccdiverge.json` and the separately run period tier rather than
converting them into passes. Branch completion still requires `make phase`
and `make byteident-ratchet`. Function-level comparisons do not replace the
strict partial-link completion gate documented in the README.

### CLI and output contract

- The optional positional build directory defaults to `EQPROOF_BUILD`, or
  the repository's `build` directory. `--layout` defaults to `modern`; a
  `build/period` argument alone does **not** select period layout.
- `--collect` runs the collector; the shell wrapper simply forwards arguments
  to this mode. `--timeout N` is a positive per-binary timeout in seconds
  (default 120). The normal census reads the existing packet.
- `--class` accepts only `DIRECT`, `COMPOSITE`, and `NONE`. By default the
  selected population excludes `EXACT`; `--grade1` selects `REGALLOC` instead.
  `--sym NAME` takes an exact symbol name, including mangling where applicable,
  and can display a symbol outside that selected population. It still requires
  the complete census inputs. These options do not make a partial build valid.
- The census groups code as `EXACT`, `REGALLOC`, `UNRESOLVED`, or `OTHER`.
  It considers every defining copy, including COMDAT copies, rather than
  choosing the most favorable one. Its aggregation can therefore differ from
  another tool's census; retain both denominators and definitions.
- `BLOB` and `TC_OUT` are inherited through `byteident.py`. The selected
  differential layout supplies test references and outcomes; code grades come
  from the separate period code-generation tree.
- The collector writes `BUILD/eqproof_checks.json` and preserves separate
  stdout/stderr logs in a unique `BUILD/eqproof-*` directory. PASS and FAIL
  section denominators are both counted. Return codes, timeouts, missing
  binaries, malformed verdicts and zero-check output remain visible.
- Collection and census return 0 for successful collected outcomes, 1 for
  unsuccessful outcomes; invalid census inputs are refused with status 2.
  A recorded zero-check run can accompany a diagnostic census, explicitly
  marked incomplete with exit 2; it can never produce a successful verdict.
  A valid full census with no symbols in the requested selection is an empty
  result, not missing evidence.
  A successful collector alone does not mean that its packet satisfies every
  provenance/freshness check required by the census.
- The census checks source/manifest completeness, object and binary freshness,
  source/blob identities, binary and log hashes, and compiler provenance.
  Keep the full build command/log and `TC_OUT/.build-config` alongside the
  packet. ELF compiler banners do not recover flags or tell which compiler
  supplied each byte of a linked binary; timestamps are not a hermetic build
  proof. The configuration must enable `DSPLIB_REPRODUCE_BUGS`.

Show the detectors firing before interpreting a clean census:

```sh
python3 tools/eqproof.py --help
python3 tools/eqproof.py --selftest
python3 tools/eqtriage.py --selftest
python3 tools/refcheck.py
```

The current eqproof self-test exercises failed and silent binaries, timeouts,
changed logs, missing evidence, a compiled-reference positive control with an
unused-extern negative control, graph reachability, and a nonexact second
definition. These validate apparatus behavior, not reconstructed semantics.

### Validation snapshot, 2026-09-10

Measured on this issue's branch over base `bcc86c61`, with reconstruction
sources unchanged. Gentoo image ID:
`sha256:c8f983568a13f34583e420a5f4c5867a86b2884d058ffbd4bc2398285325f867`.
Modern compiler: Ubuntu GCC 13.3.0; host analysis tools: binutils 2.42.
The TC configuration SHA-256 was
`7a74a062c345d90fe8ed2d55699ad2a16ce143d57303cd7f8ef59ef372e2bb7f`:

```text
image dsplibs-tc342-gentoo
flags -O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS
cxx   -fno-exceptions -fno-rtti
dcr   -O2 -fno-rerun-cse-after-loop
```

Both layout censuses compared **273/273 source objects, 375/375 test objects,
and 1,852/1,852 blob symbols**. All-copy code grades were **813 EXACT,
52 REGALLOC, 4 UNRESOLVED, 983 OTHER**. The 1,039 non-exact symbols partitioned
as **1,035 DIRECT, 1 COMPOSITE, 3 NONE** under both layouts. The potential
graphs themselves differed: period 1,943 functions/5,186 named relocation
edges; modern 1,854/4,767. Equal classification counts do not imply equal code.

After `make capture`, period collection/census returned 0 with **375/375
successful binaries, 642,353,296 checks, zero failed checks**. Modern
collection/census returned 1 with **367/375 successful binaries, 642,353,272
checks, 25,292 failed checks**, retaining the eight declared divergent
binaries. Before generating the fixture, both layouts reported the replay's
zero denominator and diagnostic census exit 2; this was useful live evidence
that a process exit of zero alone is insufficient.

`make phase J=$(nproc)` and
`make byteident-ratchet TC_EXTRA=-DDSPLIB_REPRODUCE_BUGS J=$(nproc)` passed.
The ratchet reported exact 810 → 813, register-renaming 53 → 52 against its
stored baseline, not a gain caused by this tooling-only patch. Controls passed:
eqproof **32/32**, eqtriage **11/11**, byteident **20 register-renaming and
59 relocation cases**, and **3 ratchet cases**. A service-list control removed
one 200-character symbol from the in-memory written set and verified its
complete name in the actual list output.

The full phase initially failed in the unchanged `t_v27txcreate` coverage
fixture. Its EQ-conditioning call reads the following scratch word, but the
fixture initialized only its queued prefix. Initializing both entire scratch
buffers identically fixed the unequal input; see F11200. Final phase output
reports **49,083/51,479 source lines**, **1,425 debug sites**, **35 anchored
deviation sites**, and **375 period binaries passed, zero failed**. Both
collector/census layouts were rerun after that test change.

Collector packets and their referenced raw logs are generated at
`build/period/eqproof_checks.json` and `build/eqproof_checks.json`. Preserve
them with build logs when reproducing this snapshot; they are local artifacts,
not checked-in certificates. The collector hashes source inputs, binaries and
logs; runtime fixtures such as the capture also need to be retained separately.

## Archive review and explicit identifier mapping

Historical source: tag
`archive/branches/2026-09-08/land-2026-09-01`, commit
`a1c2f9bc58007e805a62437211bfbc22746aac91`. Current review base:
`bcc86c61`, branch `tools/issue-14-equivalence-evidence`, 2026-09-10.
Review conclusion: F11200. No archive record was copied wholesale.

In the following tables **archive identifiers use `D/number` or `F/number`**
to distinguish them from current ledger citations. They mean the old D- or
F-prefixed ID at that tag, never the current entry bearing the same number.

| archive deviation | current disposition | supported material and exclusions |
|---|---|---|
| D/962 | D1500 | zero scale skips destination writes; omit archived test/mutation counts and no-caller claim |
| D/963 | D1501 | unconditional element-zero load and strict-sign negation; narrow the zero-sign claim to positive zero; no present bit-level test claim |
| D/964 | D1502 | no square root in either body; float variance versus the short routine's integer-mean residual power; basic observation already in F8464 |
| D/965 | D1503 | unsigned integer divide and wrapping machine subtraction; basic arithmetic already in F8460; decline unique C-spelling and current overflow-run claims |
| D/966 | D1504 | zero count reaches integer divide and causes #DE; decline “undrivable” and unconditional float-exception assumptions |

| archive supporting record | current disposition |
|---|---|
| F/8420 | source-layout and test claims not imported: current homes are `src/service/Beepgen.c`, `src/service/Fdspkrnl.c` and `test/unit/t_beepgen.c`; the no-caller/signature conclusions are not established by this review |
| F/8421 | verified zero constants/control flow retained in D1500 and F11200; source-loop spelling inference declined |
| F/8422 | first-load/sign behavior retained in D1501; repeated macro-sign observation already in F8460 |
| F/8423 | missing-root/tail distinction already in F8460/F8464; linked from D1502; unique reciprocal spelling and universal rounding-invisibility claims declined |
| F/8424 | machine divide/subtraction facts retained in D1503/D1504; “the author wrote no cast” is not recoverable from these instructions |
| F/8425 | no separate import: current tests already contain zero/negative converter counts; archived mutation results and blanket impossibility of other signature tests are not current evidence |
| F/8426 | signed-zero comparison limitation retained in D1501 after reading current `float_ulps`; archived bit-test result not imported |
| F/8427 | archived `t_fltutl`/mutation census not imported; those are not the current test and suite |
| F/8400 citation in the above archive prose | declined: at the tag this ID heads the toolchain makefile-conversion record, not the claimed external-caller audit; an already-misdirected reference must not be ported |
| F/9490 | proof claim declined; hazard categories retained only as review questions above |
| F/9491 | archived hazard counts and “clean implies proof” conclusion declined; current CLI does not implement that audit |
| F/9492 | compiled-reference and whole/reduced-domain distinctions retained with narrower meanings; old census, DIRECT-as-comparison and whole-domain proof labels not imported |
| F/9493 | domain-size and pointer-role cautions retained; scalar signature is not proof of a complete input domain; old cost/population claims declined |
| F/9494 | PASS/FAIL denominator lesson retained and exercised by current collector controls; historical divergence counts not imported |
| F/9495 | stale-count lesson retained as commands/provenance requirements, with no old totals; its own distinction between EXACT and UNRESOLVED also contradicts the archive headline's “byte-identical” total |
| F/9496 | partition/non-vacuity discipline retained; old cache bug and corrected census are not current implementation claims |

### Binary reproduction for the deviation review

Reference SHA-256:
`1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379`.
The five relevant function bodies and both relocated constants were inspected:

```sh
sha256sum ref/slmodemd/dsplibs.o
python3 tools/dis.py ref/slmodemd/dsplibs.o zFLTUTL_Float2Linear
python3 tools/dis.py ref/slmodemd/dsplibs.o zFLTUTL_Linear2Float
python3 tools/dis.py ref/slmodemd/dsplibs.o fComputeRMSValueFloatBuf
python3 tools/dis.py ref/slmodemd/dsplibs.o fComputeRMSValueShortBuf
python3 tools/dis.py ref/slmodemd/dsplibs.o zfFLTUTL_GetMaxAbsValue
python3 tools/tabdump.py ref/slmodemd/dsplibs.o --at .rodata.cst4:0x528 --type u8 --count 8
```

These establish binary behavior, not production reachability. The new ledger
entries explicitly separate current test-source coverage from runtime results
not measured in this review. In particular, a trapping input can be tested in
an isolated child process; skipping it in an in-process suite does not make it
unmeasurable.
