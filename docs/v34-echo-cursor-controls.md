# V34 echo cursor controls

At 20821d7c, two independently declared domains test conventional forward
cursor/count traversal against the blob, without register or layout forcing.

The [four-cell filter domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5955106212)
crosses indexed versus counted forward cursor traversal for the history shift
and dot product. The blob advances pointers and counts down. Initial period
RTL retains indexed loops; the loop pass explicitly declines strength
reduction. Its reversal detector fires on the sibling
V34EchoHistoryBackwardClean, but not V34EchoFilter: do not infer reversal
from the sibling's dump.

| Shift / dot traversal | Bytes | Verdict |
| --- | ---: | --- |
| indexed / indexed (production) | 137 | SIZE2 |
| indexed / cursor | 139 | BYTES90 |
| cursor / indexed | 138 | SIZE1 |
| cursor / cursor | 139 | BYTES82 |

Reference is 139B. Alpha comparison also fails instruction counts (reference
55 rows; candidates 53, 51, 52), so neither equal-size cell is register-only.
Only the filter changes in the two dot-cursor cells; shift-only also changes
EchoAdapt, whose alpha count differs too. Exact count stays 11/26.
The shift's taps!=1 boundary is preserved; unsigned taps=0 remains unsafe in
the original and is not turned into a newly successful input.

The [two-cell adaptation domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5955172246)
retains production and crosses to three advancing coefficient/fraction/history
cursors, with an unsigned count and the original positive-count guard.
Production 95B/SIZE16 becomes 79B/BYTES23 against 79B reference; alpha count
is 33 versus reference 34. EchoHistoryBackwardClean and V34TimingFilter also
change, with mnemonic differences against their own baselines, not only
register renaming. No exact gains or losses (11/26 unchanged). Signed-high /
unsigned-low composition, wrapping update, and high-then-low stores remain.

All six valid compiles use Gentoo GCC 3.4.2-r2, executed assembler
2.15.92.0.2, the actual saved production flags and DSPLIB_REPRODUCE_BUGS.
Both raw baselines reproduce. Complete audits cover 26 functions and 48 named
data objects: metadata, imports/exports, data values/offsets/targets and
allocated nontext controls agree in every cell. Artifacts and complete audits:
`build/playbook-v34-echo-traversal` and `build/playbook-v34-echo-adapt`.

    python3 tools/playbook_v34_echo_traversal.py --domain DOMAIN_URL
    python3 tools/playbook_v34_echo_adapt.py --domain DOMAIN_URL

Existing fixed t_v34ec apparatus has 2200 paired filter/adaptation calls:
2000 sequential calls with 32 taps and 200 one-tap near-wrap calls. These
are initialized component streams, not proven public modem histories. No
new fixture execution or source adoption is claimed for rejected cells.
Both finite traversal families are closed. Pointer-end, guard-polarity,
width/declaration/register synonyms require fresh independent evidence, not
these near sizes. F11645 and F11646 record the negative controls.
