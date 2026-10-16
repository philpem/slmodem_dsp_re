# FPM_div word-output normalization controls

F11626. Baseline `a7dfe334`; production remains 915/1852 exact functions,
94,440 exact bytes. No source, header or test changes adopted.

The 150-byte blob takes local word-output addresses, stores and reloads a
normalized mantissa, and explicitly narrows its table index. These anchors
motivate a lower-confidence inline output-helper hypothesis, independently
of the closed 32-bit division controls (F11605/F11606).

[Four-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5952949241):

| Cell | Bytes | Complete verdict |
| --- | ---: | --- |
| Retained source | 98 | SIZE52 |
| Word index | 104 | SIZE46 |
| Output helper | 144 | SIZE6 |
| Output helper and word index | 150 | BYTES105 |

No exact gains or losses. Only FPM_div changes. The helper-word cell has
43 instructions versus the blob's 46; equal size does not recover the body.
Its local count initialization occurs before the divisor-zero guard, whereas
the blob initializes at 0xa6c12 after rejecting zero.

[Three-cell initialization-ownership discriminator](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5953013284)
repeats unchanged source and helper-word controls, then moves count-zero
initialization into the output helper. The helper runs after the zero guard;
loop, counter type, scope and increment spelling stay fixed. The new cell
is 150 bytes/BYTES108, still nonexact. Both repeated controls raw-reproduce
the earlier objects. Seven valid compilations yield five distinct emissions.
Close this declared family without counter, scope, frame or loop permutations.
Reopening needs independent evidence of a different source or compiler boundary.

Each full TU has three emitted functions, zero named data objects, and one
blob-common comparison: 0/1 exact in every cell. The two table apparatus
accessors are unchanged. Symbol types/bindings/visibility/imports/exports,
allocated nontext contents and metadata agree across all cells. Unchanged
baseline reproduces the production object. Preserve the original D4 table
behavior and reciprocal-before-shift output-store order.

Replay tools/playbook_div16_normalize.py and tools/playbook_div16_output_init.py
with the linked --domain URLs. Artifacts build/playbook-div16-normalize and
build/playbook-div16-output-init preserve actual complete Gentoo GCC 3.4.2-r2
commands, DSPLIB_REPRODUCE_BUGS, executed assembler 2.15.92.0.2, RTL,
disassemblies, hashes and complete-object audits. Generators fire on known
input with four and three distinct source cells respectively. No new
differential run or partial-link improvement is claimed: no source was adopted.
The existing fixed FPM_div fixture covers all 65,536 denominators; no fuzzing
or mutation execution was used. These negative controls do not establish a
global reconstruction ceiling.
