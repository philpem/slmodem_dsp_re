# GCC3 value-carrier pass domain

Baseline3dbb1c33, updated master afterPR261/262, strict1042/1852. Independent
worktree; issue22 and other-session work untouched. No fuzzing/mutation runs.
Complete saved Gentoo3.4.2-r2 profile; bug define appended last. Unchanged
full-TU raw reproduction required before interpreting each candidate.

ADID updateLinMappMeanAndVarAlt118B original: FLDS0.5 before unsigned count
conversion and FADDP after division; retained116B uses FADDS0.5. The constant
payload is identical. F11765's independently established expression-mode
lever predicts double literal0.5 will select the original load/use boundary.
Two source cells only: baseline and literal-mode change. Restricted initial,
combine and scheduler RTL avoids known full-da ADID ICE. Require
actual addition mode evidence and complete metadata/data/relocation/bystander
review. Stop on miss; no local-result/declaration/scheduling permutation.

ParallelDifferentialDecoder<h>::process57B original versus58B retained:
MOV DL,AL; XOR memory,AL versus MOVZBL memory,EAX; XOR DL,AL. Previous
cached-state/cursor/XOR spellings are closed (parallel-decoder-retained-result).
First trace retained expression and initial/combine/later modes. Proposed
bounded new controls, if the trace supports a carrier difference: unsigned
working input and an explicit T-valued decoded result updated with XOR.
They retain output-before-state writes, all three cursors and member loop
bound. No declaration permutations or attribute/flag fit. Every affected
header consumer must be independently rebuilt/audited if a candidate wins.

After the independently exact alternate method, a finite screen of all
baseline nonexact bodies <=350B finds exactly three functions with original
FLDS half offset versus retained FADDS half offset: both ADID mean updates
and VPcmV34GetCurrentTxBitRate. F7846 concerns the distinct updateUref method, not this ordinary mean update.
The ordinary method has no claimed closed twelve-control domain. Cross the two ADID literal changes in four
complete-TU cells. No other statement/order change. VPcm is a screened
reserve, not yet attempted; broader ownership differences remain there.


The validated addition-mode lever now supplies a new discriminator for
F7846's distinct updateUref240B. It has original FLDS half plus FADDP;
retained loop-hoisted half is used by FADD without the pop and requires
extra exchanges. Prior twelve controls kept float addition mode. Four cells
cross only updateUref double-half with the independently exact two-mean
pair. Keep arrays, float reciprocal/mean, stores, calls and loop ownership
fixed. Stop on miss. updateUrefAlt's register-half pattern has no corresponding
FADDP witness and is not attempted.

The unsigned working-input proposal is held unexecuted: valid initial RTL
shows both retained and candidate XOR already in QImode. The measured
discriminator is destination/operand identity, not promotion width.
