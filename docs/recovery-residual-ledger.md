# Whole-reference residual and call coverage ledger

`tools/recovery_residual_ledger.py` inventories every reference function and
all defining copies under the canonical byte comparator. It reports the
legacy value-timing filter's exclusions instead of presenting a zero-hit
bounded screen as whole-object exhaustion. Source paths group reconstructed
inputs; they do not establish original TU ownership or service reachability.

The fresh baseline at `4169cb3b` contains 300 objects, all 1,852 reference
function names, 1,886 shared defining copies and 48 candidate-only emitted
bodies. Independent ledger regrading reproduces the canonical census and
exact-name set: 1,086 EXACT, 667 SIZE, 65 BYTES, 29 REGALLOC and five
UNRESOLVED. Each copy retains its raw verdict; the unique verdict uses the
same worst-copy selection followed by alpha comparison as byteident.py.
No detector invents a compiler-stage cause.

Of the 766 nonexact names, 386 pass the old filter, 263 fail its source-family
filter, 116 further names exceed 850 original bytes and one is at most 16
bytes. These are disjoint first-refusal counts for that screen, not a census
of the union of historical investigations.

## Previously invisible direct calls

The retained shared bodies contain 7,459 call sites: 7,314 already carry
named relocations, 95 are indirect, and 50 direct calls have no relocation.
Those 50 local targets now resolve to unique function starts in the caller's
ELF section. Both caller and target ownership must be unambiguous. Alias,
interior-address and section ambiguities remain refusals; the tool never
trusts an objdump display label as proof. The 95 indirect callees remain
unknown. Tail transfers are explicitly outside this first ledger's call-site
count; there is no complete call-graph or value-age claim.

Seven controls demonstrate a real local positive, indirect/alias refusals and
all four legacy filter boundaries. The production regrading separately
requires exactly the supplied canonical census and exact-name membership;
a missing or extra object against the manifest fails. Object/config hashes
must remain stable through generation. Without an archived build manifest,
source timestamps and source stability are checked too. With one, every
object/config/reference hash must match its archived build inputs; the
manifest describes that baseline, not the current edited source.

The initial object-only ledger was generated while the unrelated encode
source extent was being edited. Its object result is retained under
`residual-ledger-initial-objects-only.json`, with that limitation explicit.
The final replay uses the snapshotted clean baseline source/object manifest
and the strengthened stability checks. It reproduces the same census.

```sh
python3 tools/recovery_residual_ledger.py --self-test
python3 tools/recovery_residual_ledger.py \
  --objects build/tc_repro \
  --census build/recovery-baseline/byteident.json \
  --baseline-manifest build/recovery-baseline/manifest.json \
  --output build/recovery-baseline/residual-ledger.json
```

The 4 MB result remains a build artifact; the committed tool is its replay
path. Input hashes, all symbol/copy rows, unresolved/missing/extra names and
explicit refusal/eligibility reasons are retained. The broadened review
nominated original low-word/owner boundaries in the V.17/V.27/V.29 demodulators
and Class1 FIFO consumers. Their source experiments and differential results
are recorded separately, rather than credited as detector-only gains.
