# CID terminal-cursor wrap ownership

The four-cell terminal-cursor domain is closed with no exact hit and no
reconstruction source adoption. The both-terminal cell reproduces the
reference's wrap-owner LEAs, and six focused period differential binaries
pass, but its complete function remains 1,022 bytes against 1,049 bytes.
This is a mechanism control and bounded negative result.

## Question and controls

At `426814ea`, the original `CID_FSD_demodulate` derives correlation wrap
from the first loop's terminal EBX with `lea 0xa(%ebx),%ecx` at `0x9234b`
and `0x92544`, for the 7200 and 9600 coefficient arms respectively. The
low-pass equivalent is `lea 0x22(%ebx),%ecx` at `0x923d8`. Each first loop
loads a signed short, decrements its sample cursor by two bytes, advances
its coefficient cursor by two bytes and decrements its integer counter.
The second loop consumes the wrapped cursor. These are independent
operand/dataflow witnesses for terminal-derived ownership.

The closed [batch 50 factoring domain](batch50-service-cid-fsd-factoring.md)
reset pointers to `cid->ac_hist + 4` and `cid->lpf_hist + 16`. Its
duplicated-rate-arm/pointer cell is the fixed structural seed here.
Coefficient order, integer counters, rate factoring, loop bounds and all
other source statements are held fixed. The declared cross independently
chooses reset versus terminal-derived wrap for the two filters: four
cells, plus one unchanged current full-TU reproduction.

Prediction: terminal ownership should restore the corresponding LEA from
the first-loop cursor. A candidate deriving wrap from the CID object instead
would falsify that source mechanism. Complete function identity is a separate
acceptance condition.

All four cells use a valid one-past/predecrement spelling. Their first
cursor starts at `history + index + 1`; `*--h` consumes elements down to
zero and leaves `h == history`. Reset wrap uses `history + extent`, and
terminal wrap uses `h += extent`, followed by the same `*--h` reads.
For valid ring indices, the second loop ends at `history + index + 1`.
No C expression forms a before-array pointer. The compiler may bias its
machine cursor differently; the emitted `+0xa` and `+0x22` still agree with
the reference. This safe normalisation is common to all four cells rather
than a separately crossed spelling domain.

## Measurements

The frozen parent baseline is
`build/recovery-unblock/baseline` in worktree `source-recovery-next`.
All 300 object hashes match `build/recovery-packers-next/object-audit.json`.
The unchanged `Cidfsd.c` compilation reproduces the complete retained object
byte for byte, SHA-256
`9f5f6e3558b4ed5d8b7c55316f14a3e8fb72f3b4be2114c61c5fe0cb4cf2ac71`.

| Correlation wrap | Low-pass wrap | Bytes | Verdict against reference |
| --- | --- | ---: | --- |
| Current unchanged source | Current unchanged source | 795 | SIZE(254) |
| Reset | Reset | 1012 | SIZE(37) |
| Reset | Terminal | 1039 | SIZE(10) |
| Terminal | Reset | 1038 | SIZE(11) |
| Terminal | Terminal | 1022 | SIZE(27) |

SIZE is an absolute length gap, not a count of differing bytes. There are
five distinct source hashes and five distinct objects. All five full-TU
audits cover one shared function and three named data objects each. Data,
nontext section metadata, symbol inventory, binding and visibility are
unchanged. The only changed body is `CID_FSD_demodulate`; exact gains and
losses are both empty. A deliberately corrupted `.data` byte is detected
by the audit (1/1 negative control). The changed text relocation offsets
are retained in the machine record; the same three coefficient symbols
remain the targets. No other function or helper is emitted by this TU.

The both-terminal candidate emits correlation wrap LEAs at `+0xa6` and
`+0x2d4`, and the low-pass wrap LEA at `+0x134`, all with the reference's
EBX-to-ECX operands and immediate values. Coefficient cursor allocation
still differs (ESI versus reference EDI), among other whole-body differences.
Restoring the observed cursor ownership therefore does not recover the TU.

Gentoo GCC 3.4.2-r2 selected GNU assembler 2.15.92.0.2 (20040927).
The full retained flags, actual commands, source/header/object hashes,
per-cell relocation records and focused period outcome are preserved in
[the machine record](recovery-cid-terminal-results.json). The shared
experiment helper appends `DSPLIB_REPRODUCE_BUGS` after configurable flags.
The retained configuration already contains that define twice; preserving
those repetitions and appending it again changes no macro value. No
compiler option was otherwise changed.

## Differential scope and replay

The both-terminal cell passed:

```text
make period J=4 T='t_cid_fsd t_cid t_cidprog t_cidsvc t_cidleaves t_cid_mtd'
period differential: 6 passed, 0 failed
```

`t_cid_fsd` reports 13,364 checks at each rate plus eight observer checks.
`t_cidsvc` includes 735 create/reset/delete lifecycle checks and five
receiver-construction observers. The broader CID progress fixtures are
component coverage; their deliberately seeded internal verdict paths do
not establish public modem-call reachability. No fixture, tolerance or
mutation descriptor changed. The other three cells were compiled and
audited but were not differentially run. The source was restored after
the candidate run. No full `make phase`, portability, fuzzing, mutation,
strict partial-link or new whole-object census is claimed for this negative
domain; the parent integration owns its gate. Its recorded 1,091/1,852 exact
baseline receives no gain from this investigation.

Replay from the repository root with a frozen 300-object baseline:

```sh
mkdir -p build
python3 tools/recovery_cid_terminal.py \
  --domain docs/recovery-cid-terminal.md \
  --baseline-dir /absolute/path/to/frozen/baseline
python3 tools/recovery_cid_terminal_audit.py
```

Outputs are isolated under `build/recovery-cid-terminal`, including complete
candidate sources, objects, compiler dumps and disassembly. To repeat the
focused differential, temporarily install the generated
`Cidfsd/correlation-1-lowpass-1/Cidfsd.c` as `src/service/Cidfsd.c`, run the
explicit six-test command above, then restore the original source. The
failed initial Docker connection attempt is retained in the session log;
the interpreted matrix used the successful authorised Docker rerun.

Stop this domain here. The wrap operand witness is explained, while the
remaining body does not match. Reopening requires a new independent
operand, lifetime, type or surrounding control-flow witness; proximity in
size alone does not justify more pointer spellings.
