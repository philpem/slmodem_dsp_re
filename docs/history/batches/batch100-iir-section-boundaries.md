# IIR original section consumption boundaries

Baseline856c1ecb, eightcell cube: direct coefficient/state consumption by
postincrement pointers; signedshort postdecrement sectioncounter; signedshort
feedforward accumulator. Reference FPM_iir_filt advances state2 afterfirst
shiftstore and again afterrecursive-node store, advances coeff8beforeclamp
then2afterfifth read. Counter starts sections-1 and wordincrements/tests-1
with narrowing eachlatch, unlike retainedpositive intfor. Reference narrows
feedforward sum beforeclamp/output multiplication; sourceintff narrowcast at
finalexpression is optimizedaway. Preserve every multiplication/round/order
and saturationbound, no option/header/register changes. Threecomplete TU
bodies scored, fullmetadata/nontext/data/relocations/bystanders reviewed.

Domain note: positive sections equivalent; negative signedshort sectioncounts
reference loops modulo65536 while retainedfor skips, so countcontrol recovers
original count domain ratherthan assuming all-C-input equivalence. Zero
reference return register is unassigned (related D393); initialcontrols retain
currentacc_in passthrough for0, and no undefinedresult claim or sourceadoption
solely bytefit. Existingfixture excludeszero, not executedhere. No fuzz/mutation.

Independent output-ownership discriminator: reference keeps signed section
output in EDX, copies it to next-input ECX, and returns EDX, unassigned when
zero sections. Eight additional full-TU controls cross a separate int output
assigned from the short section result with the preceding cube. This is not
a padding control: it removes the invented zero-section passthrough, and
its undefined zero case must remain explicit. No adoption on size proximity.

First output cube invalid: cursor cells missed output assignment because index
spelling had already changed. Preserved as invalid-cursor-generator; no result
interpreted. Corrected exact assignment-count assertion and reran full domain.

Output ownership stage no exact gain. Final independent width discriminator:
blob signextends input/output and saturated node as word values, whereas
current node and next input are int locals explicitly assigned short values.
Eight width controls cross short input carrier, short separate output, short
saturated node on cursor+wordcount+wordff original-consumption control.
No return-prototype change, arithmetic/saturation unchanged, same undefined
zero output if separate output selected. Stop this family if no exact preimage.

Valid stages:8 section controls +9 output controls +9 width controls,26
complete TUs. No exact gain/loss. Best single filter191B BYTES15 is grade1
exact under ESI/EDI exchange (alpha_why returns None): every nonpadding
instruction/operand structure and branch matches. Separate short input and
output carriers are required in that domain; node width is raw-inert. Block
remains16B short and formI bystander changes18B residual through allocation.
No adoption: undefined zero return and negative count domain must not be
introduced solely to bank grade1. Close these source families; no pointer
declaration/register/order permutations justified. Invalid first output cube
is preserved separately and excluded from26 valid emissions.
