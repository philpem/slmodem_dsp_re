# V34 INFO1a cached value and destination capture

Baseline: `1ec83de9`, fixed Gentoo GCC 3.4.2-r2 profile with
`DSPLIB_REPRODUCE_BUGS`. Keep all interfaces, bit arithmetic and output bytes.

The original `V34GiveINFO1aBits` calls `bitreverse` at `0x85e7`, sign-extends
AX into EBX at `0x85f3`, and retains that value across its debug branch. The
reconstruction caches a full `int` and converts only for the debug argument.
The original also loads upstream (`+0x1760`) and capability (`+0x612c`)
pointers at `0x85fc`/`0x8602` before storing either byte. The reconstruction
loads the capability pointer, stores its byte, then reloads upstream. The
original stores capability first, upstream second; preserve that order.

Test six complete-TU cells: retained cache, `short` cache, or `int` cache with
explicit assignment narrowing, crossed with retained pointer uses or both
destination pointers captured before the first store. Capture upstream then
capability, following the original loads. Keep the debug call before captures.
No pointer-order, XOR, declaration-order or option sweep.

Prediction: narrowing at assignment restores the original cached word value
before the debug branch; capturing both pointers restores their value age at
the two stores. Check initial RTL and native def/use chains, not size alone.
Unchanged full TU must reproduce raw. Audit every body, call, export and data
object; stop if either predicted boundary is absent. This does not establish
a unique original declaration or a reachable pointer-alias history.

Screen exclusions: `reset_cid` already contains the original CWTL/word loop
test; its nearby debug-call window is a scheduling false positive. The corrected
Decision caller remains a broad residual (759 versus 752 nonpadding
instructions and 76 mnemonic alignment groups). An extra saved register or
different frame size alone does not justify another local-type experiment.
