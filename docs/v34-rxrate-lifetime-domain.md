# V34 receive-rate pointer lifetimes and return join

Baselineb3665999; production Gentoo config/headers retained, bug reproduction.
Original VPcmV34GetCurrentRxBitRate0x6f40 loads p3548 and pac18 unconditionally
before the role/status branches. Our pointer loads occur within the selected
arms. The original direct getBitRate call has a return, while reconstruction
uses a sibling jump. Mangling establishes a const V90Demodulator method but
carries no return type. Preserve the current unsigned return prototype/cast.

Four complete VPcmV34Main TU cells cross retained/eager two-pointer capture and
retained early returns/shared result with nested branches. No new types/maps,
changed constants, pointer offsets, padding, forced registers or flags. In the
shared-result cell both role branches set the final int, including their
ordinary ratecfg fallback; one return follows. Only session POINTER loads move,
not pointed-to reads or calls. The complete object must exist (original already
reads all these slots); no claim about invalid partial/NULL objects.

Prediction: eager lifetime restores the original entry loads and saved owner;
return sharing may explain call-vs-sibling lowering. Falsify each by actual
bodies and RTL, close the four-cell domain on miss. No declaration/literal/order
permutation afterwards. Raw baseline and all bodies/bindings/nontext-data/BSS/
relocations are mandatory; report changed bystanders and exact gains/losses.
Period gate for any adoption. No mutation/fuzzing execution.

The independent obj+4 lead remains insufficient for a new struct: original
Create clears from object0, and the anchor has no typed-control callee witness.
