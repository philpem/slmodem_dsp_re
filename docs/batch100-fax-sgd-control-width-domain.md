# SGD control threshold receiving width

Pinned 856c1ecb complete period profile. F11351 rejected changing signed cfg.sym_bits at its shared type home because pattern_det sign extends it. That finding adopted an unsigned-short receiving local only in SGD_create. SGD_control's original threshold block independently MOVZWLs sym_bits at 0x9f968; our block MOVSWLs at 0x658 and redundantly MOVZWLs the already unsigned ref_len at 0x65b. Original does not narrow the first product before multiplying ref_margin complement, unlike SGD_create.

Predeclare baseline plus unsigned-short use-site conversion in control only, represented once as a named receiving local and once as an explicit cast. No shared field or header change. Preserve original full-width product and the final short threshold store, all intervening resets and history loop. For configured symbol widths 1..16 both operands and results agree; low-word final result also agrees outside that configured domain for defined original arithmetic. Do not adopt short intermediate product merely by analogy with create. Audit all nine bodies and every named object/nontext relocation; raw fullTU baseline mandatory.

## Measured outcome

- baseline: SGD_control ['SIZE', 3]; eight bystander verdicts unchanged.
- unsigned-receiving-local: SGD_control ['SIZE', 4]; eight bystander verdicts unchanged.
- unsigned-use-cast: SGD_control ['SIZE', 3]; eight bystander verdicts unchanged.

No exact gain or loss; no source adopted. The explicit use-site cast is verdict-inert; a named receiving local changes allocation without reproducing the original. Do not reinterpret this as support for changing the shared signed field.
