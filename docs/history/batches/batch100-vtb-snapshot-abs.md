# VTB snapshot cursor and absolute-expression boundaries

Precompile856c1ecb eight complete-TU cells cross short main loop counter j,
advancing survivor snapshot pointer, and standalone standard integer absolute
captures. Original snapshot has pointer ADD4 and MOVSWL increment before CMPW7;
retained indexed node[j] and int increment with narrow comparison only. Original
coarse abs has CLTD/XOR/SUB before +0x400; retained condition distributes addition
into two branches. Standard abs capture is independent of coordinate/sign
ownership (don't mutate ri/rq, used later). No source bitmask trick or profile
change. Preserve table bytes and full function; inspect initial RTL abs versus
if_then_else. No reconstruction adoption on size score alone.

First eight snapshot/abs controls1500–1612B vs1773, no exact; families closed.
Independent original scratch-owner witness: both four-point calculation loops
store each pt to stack0x20 and0x28, each bm to0x40 and0x48, adjacent duplicated
four-word halves. Even-state4 reads bm[k+2] (no XOR/mask) and pt[best+2]. XOR2
is addition2 modulo4 on domain0..3; extended duplicate halves reproduce it.
Other XOR1/3 forms remain. Helper loops also narrow k increments and compare
word metrics. Declare12 full-TU cells: retained4-entry XOR, extended8-entry XOR,
extended8-entry arithmetic-x2 × short helper k × short ACS metric m/d.
These axes are independent original operand/scratch-use witnesses. Keep main
snapshot/abs source retained for this discriminator; no size-based permutations.

Twelve scratch/width cells1607–1853B, no exact. Independent ACS source-factoring
witness: original candidate first CMPW updates survivor, then reversed CMPW
updates chosen metric followed by MOVSWL. Retained updates both under one if.
Original branch-point load also advances pointer2 each iteration, retained bp[k].
Declare baseline plus8 controls on extended-add/word-helper/word-metric source:
split survivor/minimum updates × consuming bp pointer × retained or proved
main word/snapshot/abs source. Finite structural cross, no field-store or
register permutations. Separate comparison must come from ordinary minimum
factoring, not explicit assembly/volatile/barriers.

Nine split-min/cursor controls1813–1933B, no exact. Original odd ACS loops use
absolute state indices5..7 (four CMPW7), whereas retained loops1..3 and adds4
at old/best uses. Reference scratch reads encode actual affine subset maps:
even x0=k,x1=k^1,x2=k+2,x3=3-k; odd x0=k,x1=9-k,x2=k-2,x3=11-k.
Duplicated8-word arrays make all those indices valid and XOR-equivalent on
four-state domains. Declare raw baseline plus12 controls: relative-XOR,
absolute-loop relative-XOR, absolute-loop original affine maps × independent
minimum factoring × main snapshot/abs owners. Short helper counters/metrics,
extended scratch and consuming point pointer held fixed at independently
witnessed source. No lookup-table padding or arbitrary permuting source bodies.

Source-factoring diagnostic authorized: original eight specialized ACS loops
provide independent witness, but current general affine helper emits privately
out-of-line. Declare baseline plus8 cells: four existing affine(minimum×main)
controls, each retained helper versus literal expansion of the eight constant
calls. Do not set always_inline/noinline or change flags; no source permutation.
Ordinary literal blocks substitute only s/p0/x constants; scratch data, helper
arithmetic and statement order held fixed. This discriminates source-open-coded
versus helper-budget emission without claiming original unique source spelling.

Nine literal controls: helper985B retained privately; literal source removes
helper, decoder1622/1719B retained-main and1624/1720B owner-main. No exact1773B.
Close literal family, no alternative expansion/permutation. New independent
main-loop witness: least metric search also has separate CMPW selection and
minimum comparisons; original traceback rereads ring field after all metric/
node stores and narrows shifted ring to signed word. Stable literal predecessor
owners/minimum1/literal1 (1720B) selected before compilation. Declare raw baseline
plus6 controls: main metrics int/joint, short/joint, short/split-min × original
post-store ring field recapture with word shifted-use. No depth/header changes.

Absolute loop/formula domain results: retained relative1853/1901B, absolute-XOR
1642/1754B, affine1343B+985B private helper; owner-main relative1813/1877B,
absolute-XOR1625/1721B, affine1320B+985B helper. Second sizes correspond to
split minimum. Affine source helper inlining changes are actual compiler
behavior, not invalid runs: shared inventory guard stopped before completion,
preserved in gcc3-batch100-vtb-absolute-inventory-guard-stop. Local driver records
and permits only observed private vtb_acs, no added globals; complete13 controls
rerun/audited. An apparatus inspect/dis import-order failure occurred before
compiler execution and was corrected; not a source rejection.

Literal source expansion9 controls removes helper and reaches1622–1720B vs1773;
no source/flag adoption. Main word/minimum × ring-recapture7 controls1720–1728B,
no exact. Main int-to-short metrics alone canonical-merge at same1720/1724B;
shared main minimum is distinct. Original ring post-store read is genuine but
not a byte-exact unlock here. Stop these source-factoring families on misses.

All58 full-TU controls across6 completed stages pass audits: sole canonical
public function,32-byte VTB_DIFF_TBL contents/binding/visibility/section/offset,
all allocated nontext bytes/relocations preserved. Eight affine helper cells
(4 absolute+4 literal control predecessors) explicitly emit private vtb_acs;
metadata validated STT_FUNC/STB_LOCAL/STV_DEFAULT/.text, no extra import/global
or common bystander. No exact gain/loss (baseline decoder nonexact).

Numeric trace tool fires24 stage cases and proves all32 affine map cases:
indices stay0..7 and modulo4 equal original XOR subset for p0=0/4,x=0..3,
relative predecessor0..3. Absolute source ownership removes two initial RTL
branches and adds two SImode XOR operations, isolating standard abs lowering
from later scheduling. Literal source removes the private emitted helper.
No runtime/fuzz/mutation execution; no whole-pool exhaustion claim.
