# V8 detector boundaries, declared before compilation

Baseline 80c5dea3, complete V8Detector.c, unchanged retained period profile
and headers, -da, DSPLIB_REPRODUCE_BUGS. Baseline must reproduce raw object.
No mutation/fuzz execution; root runs phase once at batch end.

15-cell domain: baseline; phase reversal initializer field-derived twice-half
loop bound, cached twice-half bound, or short index (3 alternatives); biquad
history old/read-new/write interleaving crossed with saved-history order
(3 alternatives); detector nested section/tap clear loops crossed with int
or short counters, presence of the blob-visible three-iteration empty loop,
and table/armed store order (8 alternatives).

Predictions: the phase-reversal initializer's register-held 64 can arise from
loading its just-assigned half field rather than a literal bound. Biq history
read/store pairs recover the blob's old y/load-store followed by old x/load-store;
check whether resulting lifetime/flag pressure also explains LEA vs ADD.
Detector nested short section/tap increments and empty short loop are directly
visible in the blob. They preserve all memory effects and explain its larger
body without invented functional states. Width and table/armed order are
independent controls. All complete TU functions and relocations must be audited;
no gain alone licenses a bystander loss or ABI drift. On all misses, close these
specific source families and inspect first pass boundary before extending.

After the declared 15-cell domain yielded two independent exact functions,
combine only those winners as a sixteenth full-TU validation cell. No new
source spelling axis is introduced.

## Separate biquad call-result boundary, declared after history controls

History interleaving alone canonicalized to the baseline, and the other two
history controls missed. The earlier unsigned-short history type domain is
closed and not repeated. The remaining ADD-versus-LEA difference precedes
state accesses, so inspect the call-result accumulation boundary separately:
4 complete TUs, baseline, commuted addition, named short call result, named
int call result retaining the same short cast. The last pair separates result
storage width from arithmetic promotion; no declaration shuffles, forced
registers or flags. Prediction: if result lifetime/coalescing drives the LEA,
a named call-result boundary will alter combine/regmove before allocation.
Falsifier: identical emissions or unchanged ADD graph; close this family then.

## Measured result

16 valid main cells, 16 source hashes, 12 distinct objects. Unchanged raw
baseline reproduced. Two independent winners combine without interference:
`v8_phase_rev_init` exact77B; `v8_detectorinit` exact267B. Complete TU moves
0/6 to2/6 exact, +344 exact bytes, zero losses. All six functions retain
metadata and bindings; both named coefficient objects retain offsets, values
and relocations; allocated nontext bytes are unchanged. Only the named targets
change; source/header/type/ABI/flags remain otherwise unchanged.

Combine boundary controls (7/7): literal phase bound compares index with63;
cached bound compares index with64; field-derived bound compares invariant
register64 against index in the object's direction. It is not equivalent
spelling at compiler entry. Detector short counters leave three signed-short
counter extensions in nested/nonempty loops; retaining the empty loop adds a
fourth. The exact cell has the two-deep section/tap clear followed by the
three-deep clear containing the short empty loop, as the blob's actual CFG
shows. Integer empty loops vanish. Swapping table/armed order misses8bytes.
This is recovered structure and width, not inserted padding.

The three biquad history alternatives miss. The additional four call-result
controls are four source hashes but ONE raw object, all equal to baseline:
commutation and named int/short result storage canonicalize. The generator
is positively exercised by the 12 distinct objects and two exact gains in
the main family, so this constant map is meaningful. Close these biquad
source families; the next discriminator must trace the ADD/LEA selection
boundary and history-load scheduling, rather than another local synonym.

20/20 complete TU audits, eight causal controls including the four-object
identity control. No fuzzing, mutation execution, or behavioral harness run
in this subtask; batch owner runs the final authorized gate once on the
integrated source. Reproduction and audit output remain under
build/gcc3-batch5-v8-{detector,biquad}/.
