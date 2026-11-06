# Closed InitMOH caller-flow audit

This optional analysis compares the native and reconstructed 444-byte
`VPcmV34InitMOH`. It has a manually reviewed instruction correspondence for
this specific function shape. It is not a general equivalence detector, a
byte-grade override, or a replacement for period differential validation.

The comparison covers every reachable caller instruction, feasible path,
register/flag lane, owner-relative memory effect, stack cell, call boundary and
normal return in its explicit model. The report gives decoded/reachable/path
and obligation denominators, unsupported effects and parsed-input hashes.
Changing this function's instruction shape requires reviewing the map again;
unsupported inputs fail rather than being silently omitted.

The model assumes valid normal-return ELF32 i386 execution and the stated ABI
call contracts. Object/config/session owners may alias one another but are
constrained to an address range disjoint from the fixed debug global, string
and private stack. Matched opaque callees may write external memory and
caller-saved state after their entry arguments, memory and frame preconditions
are proved equal. Private stack preservation and callee-saved registers are
assumptions, not independently proved callee implementations. The audit does
not establish public modem negotiation reachability or unique original source.

Use a Python environment with the optional packages in `requirements.txt`.
The runner installs nothing and requires a fresh output directory:

```sh
python3 tools/native_graph/initmoh_flow_case/run_closed_audit.py \
  --native ref/slmodemd/dsplibs.o \
  --candidate build/tc_repro/src_pump_v34_VPcmV34Main.cpp.o \
  --output build/initmoh-complete-flow
```

The runner freezes both ELF inputs and four core scripts before analysis,
runs the audit, independent path-domain witnesses, normal and optimized
controls, then formats `INSTRUCTION-MAP.md` and `summary.json`. Inspect
`runner-manifest.json`, `audit.json`,
`path-feasibility.json`, and both `controls-*/results.json`. All subprocess
failures propagate. Optional dependencies are outside normal `make phase`.

Controls include accepted dead-upper-lane alternatives and rejected wrong
owners, lanes, predicates and call arguments. Malformed relocation sites,
types, addends, overlapping fields, alternate entries and unsupported effects
require their intended typed admission failure; arbitrary exceptions fail the
control. Retained AST references prevent runtime node-ID reuse from corrupting
the serialized expression DAG. No retired mutation suites or anchors are read.

See the [wave report](../../../docs/flow-retrain-recovery-20261106.md) for the
source experiments, fixture domains, remaining constraints and measured result.
