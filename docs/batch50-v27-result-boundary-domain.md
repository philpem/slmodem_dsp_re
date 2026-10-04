# V27 received-count result conversion boundary

New operand witness after the bounded consistent descrambler API cross:
RxHdxDataV27 blob retains unsigned n through branchless conditional mask,
then signed-extends only at final return. Current short r is signed-extended
before QualityDetect call and conditional selection branches. Cross only
unsigned-short result local with narrowing at signed return, independently
of consistent unsigned DescrambleDataV27 formal/signed SDM consumption.
Existing predicate, n width, forced callcast and all other statements fixed.
Narrow conversion at return preserves all 65536 result bit patterns.
Full caller/callee TUs; all other header consumers already measured raw
identical by batch50_v27_descramble_api.py. No source/header adoption merely
for extension match; whole-function exactness and bystander proof required.
