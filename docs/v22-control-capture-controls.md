# V22 control-byte sampling controls

At d672e822, an eight-function screen spans six complete TUs containing62
functions; this is not a remainder classification. V22FP_control is145B in
the blob and151B retained. The blob samples flags_0c once for both scrambler
outputs, samples flags_0d between those stores for the first HDX predicate,
rereads flags_0d after conditional HDX writes for the second predicate, then
samples flags_0c once for r20/AGC/parameter outputs. Retained source rereads
flags_0c after output stores and samples first flags_0d after the scrambler
store. Its comment says one shared byte load, but compiled evidence disagrees.

The [declared eight-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5956185675)
crosses three observed boundaries: first flags_0c capture, later flags_0c
capture, and first flags_0d capture between descrambler/scrambler stores.
The two flags_0c groups remain separate; second HDX test keeps the direct
read. All output ordering, owner accesses, constants, widths and literal
return remain. The wide parameter-flags read/modify/write remains; no type
or byte-offset overlay is introduced to fit its different blob encoding.

| Captures: first0c / late0c / first0d | Bytes | Verdict |
| --- | ---: | --- |
| 000 (production) | 151 | SIZE6 |
| 001 | 161 | SIZE16 |
| 010 | 148 | SIZE3 |
| 011 | 152 | SIZE7 |
| 100 | 153 | SIZE8 |
| 101 | 179 | SIZE34 |
| 110 | 150 | SIZE5 |
| 111 | 177 | SIZE32 |

No exact gains/losses,0/2 unchanged. Baseline and closest010 each have45
alpha instruction rows against46 reference; all111 has52. Capturing an
observed value lifetime does not recover the complete function or guarantee
its register/control layout. Only control changes across two functions and
one named14B local protocol table; complete status body/canonical relocations,
symbol type/binding/visibility/imports/exports, table bytes/offsets/targets and
allocated nontext agree in all cells. Eight valid compiles/eight raw emissions;
actual saved flags with DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and executed
assembler2.15.92.0.2. Raw production reproduces.

    python3 tools/playbook_v22_control_captures.py --domain DOMAIN_URL

Artifacts: `build/playbook-v22-control-captures` includes commands, RTL,
complete TU audit and alpha controls. No source adoption, new fixture
execution or candidate differential claim. Existing t_v22ctl constructs four
paired V22 graphs (two modes × two flags) and performs262144 paired calls,
covering all65536 control-byte combinations per graph. Controls are disjoint
from output storage: this denominator does not establish alias behavior.

Character reads can expose representation overlap with output fields, so
sample timing warrants a fixed component witness before any alias-based
fidelity claim. There is no internal caller proving the exported parameter's
original type/alias contract. Potential alias fixtures must demonstrate valid
C objects and preserve live pointers, stay labelled synthetic component
probes, and not be promoted to public modem reachability. No witness was run
in this source/codegen domain; do not report an observed behavioral failure.
The one-byte parameter update does not independently authorize bitfield or
layout changes either.

Close the finite capture family without declaration/register/store-order
synonyms or a nearest-size adoption. Any later type/API experiment requires
independent evidence and complete consumers, not these negative scores.
F11656 records the controls and fixture limits. Production remains924/1852
exact with its previously passing386-test deciding gate.
