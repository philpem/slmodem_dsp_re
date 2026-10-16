# Scalar descrambler output type

Baseline9fabd432. Original Descrambler<h,i>::process(h) returns its full result
in EAX without byte narrowing; V90Phase4Demodulator::getV90Decision forwards
that EAX to V90MP::bitsToInfo(int) twice without MOVZBL. Retained header instead
declares scalar Descrambler<T,I>::process(T) as returning T and explicitly
casts its I result to T, unlike the corresponding Scrambler method's I return.
Retained callers add MOVZBL AL and retained scalar adds a late byte extension.

Prediction: consistently returning the output template type I with no final
input-type cast removes the original-absent conversions in both producer and
consumers. Cross declaration/definition return T versus I with return(T)r
versus return r: four cells per complete consumer TU. Original function-name
mangling does not encode the return type, so mangling alone proves nothing.
Audit all transitive header consumers, body/data/exports/calls/bystanders and
weak copies. Preserve input/history type, pointer order, XOR and bulk overload.
The value range remains0..255 for byte input/history even when output is int.
No incorrect-normal-value behavior claim or unique original template spelling.

First use the complete V90Phase4Demodulator TU as discriminator. Stop or
reframe if unchanged raw baseline fails, full output return retains the two
caller extensions, or producer disagrees at a new value-use boundary. If the
prediction holds, compile all27 transitive C++ consumers across the same4-cell
domain. No method-order/variable-order/source-synonym or option sweep.

## Independent write-back witness after output-type controls

Full output return restores118-byte HI body but leaves original pointer
write-back order wrong: original stores pTap2 at+0x18 before pTap1 at+0x14;
retained scalar does the reverse. This is separate from the published bulk
decrement domains (F8046/F8142/F10210). Cross retained/full output return with
retained/pTap2-first scalar decrements over V90Phase3Demodulator and
V90Phase4Demodulator (II and HI instantiations):8 complete-TU cells. Do not
change XOR spelling, store/read ownership, pointer declarations or bulk methods.
Prediction: independent source decrement order restores those original two
write-backs; demand complete original body, not just the order or size. Close
on this finite cross; no neighbouring spelling sweep.
