# Closed Descrambler element-count transfer

At b550e9c6 Descrambler<int,int> constructor is110B versus107B blob.
Reference COMDAT+0x1b forms b+c+1, then+0x23 shifts by2; current folds
byte displacement4 into LEA. Existing Scrambler recovery explicitly leaves
this sibling untouched (scrambler-allocation-retained-result.md).

[Three predeclared header cells](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947334178): unchanged(1+b+c)*sizeof(T), reassociated(b+c+1)*sizeof(T), unsigned element-count local then sizeof conversion. First two are raw-identical across every consumer; local recovers107B but remains BYTES19. No exact gain/loss. Three sources/two complete consumer emission sets.

Fresh period dependency discovery runs over299 C/C++ sources from300-entry
manifest; the excluded input is assembly and cannot include C++ template
header.27 consumers discovered, each compiled for all3 cells (81 completed
compiles). Unchanged27/27 objects raw-reproduce production.541 canonical
function-copy comparisons per cell,314 exact unchanged.57 Scrambler/
Descrambler defining copies inspected, all unchanged except one
_ZN11DescramblerIiiEC1Ejjj in V90Phase3Demodulator.cpp. Names/types/bindings/
visibility/sections and all allocated nontext data preserved.

Count-local prefix through allocation matches reference exactly; pointer
calculations afterward retain another register colouring and restore/store
schedule. Two relocation targets and offsets now match, but19 nonrelocated
bytes do not. Recovering one arithmetic boundary does not establish full
source identity. No header adoption or arbitrary declaration/register/
statement permutations. Reopen only for another independently supported
boundary or compiler mechanism.

Tools/playbook_descrambler_count.py uses original full source paths and
first-priority header overlays, configured saved CXX flags, mandatory
DSPLIB_REPRODUCE_BUGS, Gentoo3.4.2-r2 and selected executed assembler
2.15.92.0.2. Commands/dependencies/header/source/object hashes/full inventories
and all symbol records retained in build/playbook-descrambler-count.
Runtime, retained whole-tree census and partial links NOT RUN: no adoption.
No fuzzing or mutation execution. F11581.
