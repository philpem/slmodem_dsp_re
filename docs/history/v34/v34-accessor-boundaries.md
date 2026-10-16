# V34 accessor recovery through switch and arithmetic boundaries

Issue [252](https://github.com/philpem/slmodem_dsp_re/issues/252), baseline
master3f8b8f77, isolated branch investigate/v34-snr-loop-boundaries.
PR251 is merged. The concurrent PR245 is left untouched: V90 constructors,
core dp_param/Fdsp, V32 FSE/E2u and rd/shaper are outside this workstream.
Its F11694 dp_runtime finding conflicts with landed F11694 V8 CRC and must
be renumbered when integrating that draft. F11700/F11701 are reserved here.

## Quick-connect: recover the source switch, not its masks

Blob VPcmV34GetQuickConnectIndication is59 bytes versus73 retained. Both
have unsigned0..10 range guard and masks E7,408,310, but retained early
returns create different branches and a SETNE tail. Three declared cells:
production; grouped switch with early returns; grouped switch with result
initialized zero, assignments/breaks and one return. The switch-return cell
reaches59 bytes but remains BYTES55. Only switch-result is EXACT59.

The grouped source returns is_short for states0,1,2,5,6,7; zero for3,10 and
default; one for4,8,9. Negative and large states retain zero. It reads the
same fields and performs no store. A single initialized result supplies the
shared epilogue seen in the blob.

[GCC3 stmt.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/stmt.c)
`emit_case_bit_tests` converts grouped cases into masks; `case_bit_test_cmp`
sorts by number of case nodes, not number of covered integer values. Adjacent
labels merge to ranges: each group has two nodes, consistent with the mask
order in the blob. Initial RTL already holds unsigned guard and three AND
masks in that order. Thus the masks do not establish author-written bit tests.
No unique ordering of the original case labels is claimed.

## SNR: preserve the old value until the product exists

Blob VPcmV34GetSNR is99 bytes versus90 retained. The division prefix agrees;
it does not exhibit updateAlpha's spill problem. The blob initializes a
previous value in ESI, retains it through first-loop exit and moves it into
EAX for the second loop. Retained nested guards let that lifetime collapse.

First domain: production; first loop flattened to while(v>0), v initialized
zero before the divisor guard; then the same first loop with explicit
v=last handoff and second while(v>0). Flat-first raw-merges production.
Flat-handoff recovers99-byte length but remains BYTES33, zero gains.
It still saves last before destructively multiplying v.

The independent instruction fact is blob IMUL oldv→product, copy oldv→last,
product→v, SAR v. A declared two-cell continuation retains flat-handoff and
moves product calculation before the old-value save:

```c
while (v > 0) {
    int product = (int)((unsigned int)v * 0x1013u);
    last = v;
    v = product >> 14;
    if (v > 0)
        db += 6;
}
v = last;
```

This is EXACT99 under unchanged flags. Unsigned wrapped products and signed
shifts/count timing/divisor guard remain intact. Product-before-last is
visible in initial RTL and survives combine: UID62 multiplies v64 into named
product72; UID66 subsequently copies v64 to last63. The old v dies at that
copy, so it cannot be overwritten by the earlier multiply. In the negative,
combine uses last63 as the multiply input (UID66) after its prior copy.
This is a source-use boundary before allocation, not register-name fitting.
The natural loop representative is supported, not uniquely recovered.

## Full-TU, production and fixed-gate validation

Final two-cell domain confirms both independent exact candidates coexist.
10 valid compile cells,7 distinct sources,6 raw objects; no invalid compiles.
Both first-family baselines raw-reproduce production. Product-domain baseline
raw-reproduces preserved flat-handoff; combined baseline raw-reproduces
production. Same Gentoo GCC3.4.2-r2/saved profile, selected executed assembler,
mandatory DSPLIB_REPRODUCE_BUGS last; commands/hashes/config are recorded.

All10 full TUs audited:57 functions/1 named data object, unchanged metadata,
named data, allocated nontext and canonical relocation targets. Mask/product
positive and negative controls pass3/3;48 selected stages parse with instruction
UID/patterns, excluding metadata. Only the two named bodies change in combined
cell:23→25 exact/57, zero losses. No bystander body change or anchor edits.

Fresh baseline300/300 objects raw-match prior930-symbol census. After adoption,
only VPcmV34Main.cpp.o changes; full raw object equals combined candidate.
Whole-tree930→932/1852, exactbytes95823→95981 (+158), exactly two gains,
zero losses. Fixed `make phase J=4`:388 passed,0 failed, structural boundary
passes;285 suite declarations/10038 static anchors clean. Existing component
QuickConnect297 and SNR681 fixed checks pass; no new lifecycle claim.

Same-order partial before/after: positioned equal bytes68908→68923/943398;
exact relocations1028/18317 and symbols394/2907 unchanged. Both complete-object
verdicts remain DIFFERENT. No flag changes, fuzzing or mutation execution.

## Replay and transferable levers

At baseline3f8b8f77, build make tc and archive tc_out as production-before.
Run tools/gcc3_v34_accessor_reproduce.py with --family quick and --family snr,
--baseline-dir build/production-before, --domain issue252 URL. Then run
--family snr-product using domain comment5975365637; it automatically prepares
the preserved flat-handoff object/config as its control. Finally --family
combined with production-before and domain comment5975376952. Run
`python3 tools/gcc3_v34_accessor_audit.py` for metadata/data and stage controls.

Apply the switch-result lever where literal masks reproduce a state partition
but disagree in branch/epilogue structure. Confirm GCC's initial expansion
before calling the masks source statements. Apply the product-use lever where
an old value is still consumed after a multiply in the blob: trace its source
use and death before trying an allocator explanation. Neither lesson permits
arbitrary declaration or register permutations in already closed families.
