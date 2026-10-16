# Data-mode field ownership and type audit

The retained change models `v22fp_hdx::rms_accum` as `unsigned int` and
removes its compensating unsigned arithmetic casts. This is a byte-neutral
representation of the original's observed arithmetic. The original declaration
is not uniquely recovered: signed storage with explicit unsigned casts remains
a possible source. There is no byte-identity gain.

## Original writers and readers

The reference is `ref/slmodemd/dsplibs.o`, source baseline
`eccf0f2ae817cec09e93886ed7635604600a75dd`. Full original bodies were read with
`tools/dis.py` for `V22FP_create`, `v22_originate`, `fskdemodulate` and
`receiver`. Field ownership follows the pointer established by the function,
rather than matching an offset across unrelated objects.

For the V.22 half-duplex owner at `fp+0x50`, the complete source access census
contains one initialiser and three arithmetic uses:

| Original address | Operation on half-duplex owner |
| --- | --- |
| `0x87c0f` | Constructor stores dword zero at `+0x2c`. |
| `0x8bbb8..0x8bbc4` | Zero-extends the word at `+0x28`, adds it to dword `+0x2c`. |
| `0x8b724..0x8b737` | Loads dword `+0x2c`; zeroes EDX and performs unsigned DIV by the zero-extended word block count. |
| `0x8b739..0x8b748` | Compares quotient to zero-extended threshold, writes the full quotient back to `+0x2c`, and uses unsigned JBE. |

`rg rms_accum src include test/unit` found production accesses only in
`V22.c` and `v22mod.c`. The remaining named occurrences are documentation and
fixture setup. No field address escapes, signed comparison, negative writer,
or overlapping word view was found. The change retains the 32-bit extent and
every neighbouring offset. Unsigned storage also expresses the original dword
addition's wrap semantics directly.

Every production header consumer is in `src/pump/v22`: direct inclusions are
`V22.c`, `V22int.c`, `v22.c`, `v22mod.c`, `v22prc.c`, `v22rxtab.c` and
`v22stc.c`; header edges through `v22rate.h`, `v22hdx.h` and `v22prc.h` were
included in this review. The experiment conservatively compiles all 14 V.22
translation units under both representations.

## Experiment and validation

The domain was declared before compilation: retained signed representation
versus an unsigned field plus ordinary unsigned divide/compare. The prediction
was byte neutrality across every consumer. Any changed body, data, relocation,
export or binding would falsify that prediction and require inspection before
retention. The candidate generator checks the exact original text and archives
the distinct source and header hashes for each cell.

`tools/recovery_data_fields.py` uses the shared full-TU driver and
`experiment_toolchain.compile_shell`, preserving the full Gentoo profile and
appending `DSPLIB_REPRODUCE_BUGS`. The recovered GCC 3.4.2-r2 driver and its
selected GNU assembler 2.15.92.0.2 were executed for identity. Actual complete
commands, input hashes, object hashes and per-symbol verdicts are retained in
[the machine record](recovery-data-fields-results.json).

All **28 compilations** succeeded: **14/14** unchanged objects raw-reproduce
the frozen baseline and **14/14** candidate objects are raw-identical to those
controls. This compares complete object bytes, including nonexact function
bodies, relocations, data, symbol inventory and binding. The shared function
denominator is **61**, of which **27** are EXACT against the reference before
and after. Gains, losses, missing symbols and changed bodies are all zero.
The frozen tree-wide baseline's 1,093 exact symbols are preserved by these
identical replacement objects; this is not a newly executed tree-wide census.

Focused Gentoo `make period J=4 T='t_v22fpcreate t_v22org'` reports
**2 passed, 0 failed**. Before interpreting it, the fixture was reviewed:
`t_v22fpcreate` exercises construction; `t_v22org` creates the real object
pair and runs real DSP callees, but also plants node/counter states and alters
slicer controls. Its arm coverage is a synthetic component-handler fidelity
probe, not evidence of public modem reachability. It explicitly avoids the
zero-count divide input. No fixture, tolerance or helper signature changed.
An earlier misspelled test selection `t_v22fp` correctly failed selection and
ran zero tests; it contributes no validation result. `make phase` and strict
partial-link comparison remain the parent integration gate.

Reproduction after retention uses the explicitly archived headers:

```sh
python3 tools/recovery_data_fields.py --historical-headers \
  --domain 'Unsigned V22 RMS accumulator versus retained signed field' \
  --baseline-dir /path/to/frozen/baseline
```

## Closed historical leads

> 🧠 **From Hindsight memory (Issue #260 structure cleanup: typed members over raw offsets)** — four offsets were listed as unverified leads. Each was rechecked against current source and original instructions below.

| Lead | Fresh evidence and decision |
| --- | --- |
| `fskdemodulate` state `+0x12` | Already `unsigned short sr`, with word stores and zero-extended reads in the current map. Original `0x73d86`/`0x73e10` reads and `0x73d96` write agree. No missing refactor. |
| `fskdemodulate` state `+0x06` | Word `bit_hi` at `0x73d8a` feeds OR and a truncated word store. Zero extension here does not establish unsigned storage: upper bits cannot survive the store. The alternative `+0x04` arm has the same graph. Keep the existing short. |
| `receiver` state `+0x214` | Word error store `0x5c60a`; zero-extended read `0x5c74c` feeds low-word addition/store at `0x5c76a`; CWTL at `0x5c774` establishes the signed low-word value for squared energy. Existing short is compatible. |
| `receiver` state `+0x216` | Word error store `0x5c619`; zero-extended read `0x5c745` feeds low-word addition and word store `0x5c78e`; MOVSWL at `0x5c771` establishes the signed low-word value for squared energy. Existing short is compatible. |

`rx_predict` is a static reconstruction helper; the original predictor graph
is inlined in `receiver`. Its source prototype supplies no independent original
callee-type evidence. These four leads reopen only with a new writer, a
full-width consumer, or independently typed boundary contradicting this map.

A bounded V.23 transmitter padding check also found word-only `remaining`
initialisation at `0x872ef`; there is no dword constructor-write contradiction
supporting replacement of `remaining` plus `pad0a` by an integer. This does not
claim a complete V.23 field audit.
