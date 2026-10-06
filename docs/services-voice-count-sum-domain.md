# Voice final heterogeneous count sum operands

Basee0052eec, two complete src/voice/voice.c TUs. Original voice_modem after
its handler loads det_len using MOVZWL into ECX, saved handler count using
MOVL into EDX, then ADD EDX,ECX to publish final count. Retained body differs
only in those two destinations, driven by saved+det_len source. Test exactly
one alternative det_len+saved; no type, ABI, initialization or declaration
change. Addition of promoted unsigned shorts cannot overflow signed32 and
final short publication is identical over full65536×65536 input domain.

This is an operand boundary witness across two different output values,
independent of F11774's closed detector formal-width/caller-cast controls.
A complete raw exact hit is required; no follow-on register/scope permutations.
All emitted bodies, symbols, allocated nontext and relocations audited.
