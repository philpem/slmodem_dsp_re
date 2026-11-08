# Waves 34–35: owner evidence and partial visual-diagnostics recovery

Base: `19372c521b1ade955a00e09863167ec2be2aaa32` (PR #413). Issue
[#409](https://github.com/philpem/slmodem_dsp_re/issues/409) records the
predeclared visual-diagnostics family. The INFO0 alias question remains open;
this pass retains a separately witnessed partial source recovery in
`VPcmV34GetVisualDiagnostics`.

## Wave 34: owner interfaces do not settle the INFO0 alias declaration

Native instructions corroborate the session class and neighbouring interfaces:

- INFO0 reads session `+0x6120` at `0x81ae`, stores a short to receiver `+0xabcc`
  at `0x81f9`, and reloads the session field at `0x820b`, without an intervening
  call on that path.
- Its caller at `0x6e764` passes the V.34 owner and owner `+0xa97c`, not a
  separate session argument or a negotiation-member pointer.
- The pointer loaded from owner `+0x3548` reaches the mangled
  `VPcmFloModem::setPcmSessionType(int)` call at `0x6b84`.
- The neighbouring `getUinfoValue(short)` call at `0x8dbc` receives the narrowed
  `+0xabcc` value loaded at `0x8d69`.
- The enclosing owner and session have separate allocation histories (`0x3a49`
  and `0xfd9f`, followed by the session constructor at `0xfdcd`). The session
  selector store at `0xfcbb` derives from a `V90ModemSide` constructor argument.

Those are physical-owner and typed-interface witnesses. They do not establish
the declarations available to INFO0's optimiser. All four INFO0 calls target
`dsplibs_debug_printf`; no external access helper supplies the reload boundary.
The diagnostic names the negotiation values but supplies no union declaration.
Seven supplied reference headers contain no relevant declaration, and the ELF
has no debug-type sections. Reconstruction headers cannot independently witness
their own assumed layout.

The bounded inventory covers 3082 symbols, 1852 function entries and 18317
relocations; 42 selected signatures (34 mangled/eight unmangled); 60 literal
displacement hits across 24 owners; 31 complete bodies and two handshake ranges;
30 strings, four reconstructed histories and seven supplied headers. It produces
no new alias-declaration witness or justified source family. No compilation or
INFO0 change follows from this negative result. The preceding union/cast and
nearby syntax families remain closed.

## Wave 35: recover native visual-diagnostics boundaries

The refreshed structural queue also selected `VPcmV34GetVisualDiagnostics` in
`src/pump/v34/v34diag.cpp`. Native disassembly independently witnesses:

1. **Selector-8 exclusion:** `0x72b6`–`0x72cd` checks the status range that returns
   zero before the status/role branch that calls the K56flex getter. The prior
   conjunction source did not retain that range test.
2. **Echo input timing:** PCM float load/advance at `0x7270`/`0x7272`, far-short
   at `0x7456`/`0x7459`, and near-short at `0x7490`/`0x7493`. Each reads the input
   before the output real-component store and advances its source pointer.
3. **Equaliser owner:** signed two-byte loads at `0x7364`/`0x736c` use receiver
   offsets `0x50c`/`0x5ac` directly, rather than an intermediate equaliser base.

The declared family crosses range/conjunction control (two forms), indexed/
postincrement/original-type captured echo reads (three), and derived/direct
equaliser owners (two): **12 candidates plus unchanged control**, **13 successful
objects / 39 function verdicts**, zero compiler rejections and eight distinct
emissions. The unchanged object is byte-identical to production, including all
three bodies and its complete ELF ledger.

The retained partial combines the explicit range branch, `float`/`short` value
capture from `*coefs++` before output stores, and direct
`rx->equalizer.re[i]`/`im[i]` accesses. Direct postincrement and captured forms
emit identically, so neither syntax uniqueness nor an exact preimage is claimed.
The capture makes the independently witnessed input-before-output order explicit.

### Changed-use address proof and collateral

Thirty-one period-compiler layout assertions pass. The changed-use ledger checks
owner, complete byte offset, index scale and access width, not just struct size:

| Access | Prior address | Retained address / width |
|---|---|---|
| Equaliser real | receiver `+0x3cc+0x140+2*i` | receiver `+0x50c+2*i`, signed short |
| Equaliser imaginary | receiver `+0x3cc+0x1e0+2*i` | receiver `+0x5ac+2*i`, signed short |
| PCM echo | session pointer at owner `+0x3548`, coefficient pointer `+0x6bf0` | same owner/offset, float, advance 4 |
| Near echo | owner coefficient pointer `+0x80c0` | same owner/offset, short, advance 2 |
| Far echo | owner coefficient pointer `+0x9140` | same owner/offset, short, advance 2 |

The receiver starts at owner `+0x264`, so the equaliser object-relative offsets
remain `0x770`/`0x810`. Selector status remains a four-byte owner `+0x0` read and
role a two-byte `+0x359c` read. Counts, clamps and eight-byte output stride are
preserved.

Only the target body changes. Both neighbours are unchanged: non-exact
`VPcmV34GetDiagnostics` and exact `VPcmV34GetSNR` (99 bytes). There are no new or
missing functions, binding changes, external call-target changes or grade-0-or-1
gains/losses in the matrix. The complete ledger records changed jump-table
destinations; constant/string payloads remain unchanged. Parent production
compilation reproduces all three reviewed candidate canonical bodies after
source formatting.

### Partial result and residuals

The target changes from **987 to 1006 bytes**, against the native **1023**. The
reason for retention is the three independently witnessed boundary classes,
not the smaller size gap. No candidate in the declared family is exact.
Residuals include block/tail layout, native PCM subowner formation (`+0x6bd0`
then count/pointer `+0x14/+0x20` versus flattened offsets), equivalent resampler
base choices and register/argument preparation. They are recorded rather than
attributed to an unmeasured compiler stage or compensated with scratch padding.

Whole census remains **1197 grade-0-or-1 / 1852 names**; exact and grade-0-or-1
gain/loss sets are empty. Partial-link comparison remains **DIFFERENT**, including
raw bytes: positioned reference matches move from **76787 to 76775 / 943398**;
candidate `.text` grows from 711399 to 711415 bytes. Exact-position relocation
matches stay 1160/18317 with 18301 candidate records. Those counts do not assert
unchanged relocation positions/targets or partial-object equivalence.

## Validation and evidence

Independent read-only review verified the original instructions, finite matrix,
address/width proofs, complete-TU collateral and partial claim before adoption.
Focused `make period T=t_v34diag J=2` passes **1/0**. Final `make phase J=8` passes
**394 period binaries, zero failures**, with all structural checks passing.
The retained Gentoo GCC/G++ 3.4.2-r2 flags and executed assembler 2.15.92.0.2
(20040927) are recorded; `DSPLIB_REPRODUCE_BUGS` is appended by the shared helper.

Artifacts under `/mnt/zfs/dev/slmodem/scratch/independent-recovery/`:
`w34-owner/report.md`, `w35-visual/REPORT.md`, its matrix/ELF/access ledgers,
`w35-visual/final-verification.log`, before/after partial JSON/logs, final census,
focused-period log and `w34-35-phase.log`. The session handover records delivery.
