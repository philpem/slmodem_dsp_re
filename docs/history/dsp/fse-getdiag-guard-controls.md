# FSE_getdiag case1 positive-guard control

Blob229B/current213B. Case1 separately tests n>0 before its loop compare,
while case0 has no analogous extra pretest. Original
[loop-only domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5948676101)
was followed by a false branch-target clarification: it mistook0xa7d86 for a
zero-return site. Full disassembly proves mov EBX,EAX there, so nonpositive
selected counts pass through. Existing t_v32fpsub already tests that behavior.

[Authoritative corrected domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5948728529)
returns to the original two cells: baseline vs if(n>0) around onlycase1's loop,
return n outside; clear before guard, cap/case0/all other source unchanged.
Both complete objects distinct; raw baseline reproduced. Candidate217B,
not229B. All4 functions/5 globals/nontext/type/binding/visibility preserved,
only getdiag changes, exact1/4 unchanged. No source adoption or candidate
runtime/census/partial gates. Fixed fixture lives in t_v32fpsub, not t_fpm_fse;
its planted negative log counts are component fidelity probes, not lifecycle
reachability evidence.

Two earlier compiles under the incorrect return clarification are preserved
INVALID/excluded in build/playbook-fse-getdiag-guard/invalid-return-boundary.
Wrong candidate178B would return zero for negative selected counts and cannot
be the preimage. No candidate runtime/source adoption occurred. Corrected two
cells compiled separately with actual saved domain URL, full Gentoo flags and
mandatory bug define/selected executed assembler2.15.92.0.2. Artifacts retain
commands/hash/inventories/symbol/nontext audits/disassembly/RTL. Tool
playbook_fse_getdiag_guard.py. F11593.

Read the actual instruction at a branch destination before inferring a return
value. Close this loop-only guard family without source/score fitting.
