# Closed FloatFIR coefficient-update controls

At599f0fa4 setCoefficients blob58B/current56B. Reference0x46db8 loads
index, min-selects index/room at0x46dbf–0x46dc1, always stores at0x46dc3.
Baseline conditionally stores and has early equality return/second xor-zero.

[Four predeclared complete-TU controls](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947530136): baseline56B/SIZE2; min ternary store58B/BYTES26; nested update instead of early return54B/SIZE4; both56B/SIZE2. Four sources/four emissions;2/8 exact unchanged. Only setCoefficients body changes; all8 functions/8 global bindings/types/visibility/nontext data preserved.

No src adoption. Recovering unconditional store and byte length still leaves
register/scheduling differences; nested control is not an unlock. Do not
expand arbitrary variable/store/definition permutations. Runtime, retained
whole-tree census and partial gates NOT RUN for these unadopted candidates.
No fuzzing/mutation execution. F11583.

Full configured CXX flags+mandatory bug define, Gentoo3.4.2-r2 and selected
executed assembler2.15.92.0.2, raw unchanged full-TU replay. Actual commands,
hashes, inventories/symbol records/changed disassembly/RTL saved in
build/playbook-floatfir-set. Replay tools/playbook_floatfir_set.py --domain
URL above.
