# V34 DFT original unmasked shift diagnostic

Declared raw902 baseline and one unmasked-original count control only.
Reference movzbl scale then shl contains no source&31. Current source documents
its added&31 to define hardware count wrapping at C level. All8 reference
relocation call sites prove scale5/2/5/2/6/2/6/2, respectively calls0x5e96d,
0x67da3,0x692e7,0x6963e,0x69cec,0x69d03,0x6a6da,0x6a6f1. Full-object aligned
disassembly proves materialization and third argument store; arbitrary byte
slice produced misaligned garbage and is retained excluded under /tmp with
-aligned replacement. Source callsites4 statements (one helper inlined) use
5/6/2/2. Earlier source comment's4 is actually a bin count, not scale.

Candidate removes only&31 from unsigned-char working shift, preserving byte
conversion. Domain equivalence: low byte0..31; outside it C shift is undefined
while reference/currentmask remain defined hardware behavior. This distinction
must remain explicit, and no source adoption solely for byte gain. Complete TU
rawbaseline with --historical-headers because independent cosine prototype
already adopted, actualflags/bugdefine/assembler, metadata/data/nontext/relocs/
bystanders. Root decides whether original-source recovery contract supports
adoption; no harness/mutation/fuzz run.

Measured result: Two complete TUs: dftenergy unchanged74B vsreference86B/SIZE12,2/3 exact unchanged. Entire canonical map inert; no adoption.
