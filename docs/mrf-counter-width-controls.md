# MRF loop-width investigation

After F11667, revision cbd16911 returns the blob's unsigned result word. The remaining loop-body mismatch has independently observed scalar boundaries. This is an ongoing investigation; no local-width candidate is adopted.

The [first two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5958654817) changes only int produced=0 to short produced=0. Both full-TU compiles are valid and the raw baseline reproduces production. The helper changes446→447 bytes versus blob533. Candidate CWTL at+0x1fe before saving the next counter restores the blob's increment boundary at0xa9053; the output indexes the old sign-widened count. Instruction store scheduling differs and supplies no independent source-order constraint. Init193/free16 remain exact; strict2/3 remains unchanged with0gains/0losses. Full3function/1data metadata, imports/exports, allocated nontext and canonical relocations agree. No runtime or source adoption is claimed. Artifacts: build/playbook-mrf-counter/{results.json,complete-object-audit.json}; tool tools/playbook_mrf_counter.py.

The [next four-cell cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5958701315) combines this counter axis with a separately evidenced scalar-state family. Remaining subtraction narrows at0xa8fa8; phase add/sub narrows at0xa9010/0xa9034; need increment widens its low signed word at0xa902f. Ring next narrows BEFORE comparison at0xa8f67/0xa8f6a and0xa9094/0xa909b. The tested family makes branches/decimate/history length and phase/widx/need/remaining short, and uses short advance arguments/result with a short next before the comparison. Convolution k and accumulator remain int. These source types are a bounded hypothesis, not uniquely inferred declarations. Four cells are baseline, short-produced, short-state, and both. Shared API, source statements/order and saved compiler flags remain unchanged. All four valid compiles raw-reproduce their baseline and have four distinct emissions. Sizes are446/447/492/494 versus blob533; exact2/3 remains unchanged, without gains/losses. The combined cell restores every targeted signed-word update/comparison. Only filter changes;3 function/1 data metadata, allocated nontext and canonical relocations agree. Actual signed input and unsigned result boundaries remain unchanged. Artifacts: build/playbook-mrf-state-width/{results.json,complete-object-audit.json}, /tmp/mrf-state-width-stage2-audit.txt. No source or runtime adoption.

Safe runtime boundaries remain the actual initialized fixtures, at most32768 outputs for the fixed10:9 high-count fixture. Do not use invalid wrapped negative output indices as reconstruction evidence. No fuzzing or mutation execution. Production still925/1852; committed cbd16911 phase387/0 and both GitHub jobs passed.


F11668 records six valid compiles across the two domains, four distinct source/emission combinations, with no byte-exact gain. Close this local-width spelling family. The next independent source boundaries are the needed-input private short countdown (original word increment/test/nonzero versus current positive guard/32-bit decrement), shortfall nonzero versus positive predicate, and branchless ring wrap (blob SETL/NEG/AND versus candidate conditional branch). Preserve original need for subtraction and the convolution int counter. These observations justify new posted domains; no further compile is authorized by the closed width domain. Invalid negative-history or wrapped-output fixtures are not introduced.


The [next nine-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5958780568) has an unchanged production control plus eight narrowed candidates crossing private needed-input countdown, shortfall nonzero test and in-place conditional-zero ring wrap. GCC3 noce_try_store_flag_mask supplies the latter discriminator through destination identity (Playbook F11568); the mask instructions do not uniquely establish explicit author-written masks. Raw narrow baseline must reproduce the494-byte object before interpretation. All nine compiles are valid and yield nine distinct emissions. The raw production control446B and narrow control494B reproduce their prior objects. Needed countdown and shortfall nonzero independently restore MOVSWL/word INC/JNE; in-place clearing independently restores both SETL/NEG/AND wraps. No complete function becomes exact: strict2/3 for every cell, no gains/losses. Only filter changes; all3function/1data full-object audits agree. Source/runtime adoption is declined. F11669 records this domain.


| Narrowed candidate axes | Filter bytes (blob533) |
| --- | ---: |
| Control |494|
| In-place wrap |515|
| Shortfall nonzero |482|
| Shortfall + in-place |503|
| Needed countdown |494|
| Needed + in-place |501|
| Needed + shortfall |482|
| All three |489|

Equal lengths do not mean equal bodies: all nine emissions differ. In-place conversion uses XOR-zero before SETL rather than blob MOVZBL, with further widening after AND. Do not turn the closest length into source recovery or expand adjacent types/order based on scores. Artifacts: build/playbook-mrf-loop-boundaries/{results.json,complete-object-audit.json}. Close this family and reframe toward an independent function/source boundary; no author-written mask conclusion, source adoption, runtime claim or extra exact count.
