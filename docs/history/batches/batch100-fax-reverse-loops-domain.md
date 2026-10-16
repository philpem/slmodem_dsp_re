# Original byte-reverser decrement and narrowing boundaries

Base856c1ecb retained full period command/config/bug define/executed assembler,
raw completeTU baseline. No earlier byte-reverse family controls in Playbook
or batch50domains; FCS postdecrement controls are separate and stayclosed.
Original faxvmi_byte_reverse (93B) outer guard computes decremented short
count then tests old count by INC AX/zero at entry/tail. Inner counter starts
7 after first postdecrement of8, stores decremented counter with MOVZWL and
uses INC AX/zero, versus source signed >=0 loop. Original accumulator shift
narrows at MOVZWL AX before OR with bit, versus current narrow after OR.

Three independent witnessed source boundaries: signed outer short postdec,
unsigned-short inner8bit postdec, shift accumulator word then OR separately.
Full8cell cross, allbufloads/pointerwalk/store widths and input shifting fixed.
Exact original low8bit reversal semantics for every65536inputword; signed
outercount bitpattern wrap preserved including negativecount. No loopwidth/
statement/register/permutation/flag/header variations. FullTU allmetadata,
nameddata/nontext/relocations/bystanders/exactsets audited. Stopafter finite
family; shared original frame-wrapper literal-body lead separate discriminator.
No phase/fuzz/mutation/runtimeexecution; root finalgate.
