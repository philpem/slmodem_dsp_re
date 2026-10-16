# SineWave cold-exit layout attribution

Pin2180f9d0, fixed Gentoo profile/current headers/mandatory bug define.
Retained generate134B/BYTES50 has original nonbranch operand multiset; zero-trip
cleanup joins wrap before return, while blob places cleanup after return.
Closed scalar/trig/publication permutations remain closed; no branch hints,
forced registers, layout gotos/padding or production profile changes.

Prepare unchanged complete VpcmFloModem TU with -v -save-temps -da and require
raw production object equality. Trace CFG, prediction, block reordering and
x87 stack conversion, preserving basic-block indices/edges/probabilities and
UIDs. Existing selected control predicts entry50/50, exit10%; STC joins zero
block to wrap. Prior ordinary common-loop control predicts loop header64/36
but reads phase before guard. Sources explain retained decisions, not original
RTL/profile. Verify read-only graph attribution with original machine blocks.

Independent witness: original skips phase publication on the first iteration
and reads first phase only after successful count guard; subsequent iterations
publish carried x before fsin/output. A natural counted loop can express this
first-iteration owner with if(i==0) x=phase; else phase=x, followed by output and
unconditional advance; after zero iterations acquire phase before final wrap.
Two full-TU cells only: unchanged and this explicit first-iteration ownership.
Predict ordinary loop header heuristic plus original first/zero-trip read paths;
falsify on retained conditional in hot loop, changed memory/rounding graph,
missing final advance or whole-TU/differential failure. No follow-up spelling
matrix if it fails. Do not infer original hint/profile from mere byte gain.
