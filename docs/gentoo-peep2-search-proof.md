# Installed Gentoo scratch-search proof

The constructor and EpochDetectV29 controls agreed through allocation/reload
and first differed at peephole2. A read-only GDB observer now measures the
installed compiler's persistent search cursor, candidate sequence, live and
reserved bitmaps, register-class/mode checks and returned scratch. It also
records the generated matcher's replacement result and the input RTL pattern.
No inferior function calls or source, RTL, register or cursor writes occur.

The image's unchanged C and C++ compiler binaries are pinned separately:

| Binary | SHA256 |
| --- | --- |
| cc1 | `80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc` |
| cc1plus | `b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d` |

The search function has the same instruction offsets in both installed
binaries; function and cursor addresses differ and are separately pinned.
The observer stops after the prologue because optimized DWARF argument values
at that point are stale: entry arguments are read from the measured ABI frame.
Enclosing frames include a generated `gen_peephole2_*` function and a
`peephole2_*` matcher, not necessarily `peephole2_insns` itself.

Six saved complete-TU inputs: fax retained/split-tail/retained-repeat and
historical constructor pre-retype/post-retype/pre-repeat. Each replay uses
its saved complete cc1 command and preprocessed input; original preprocessing
includes bug reproduction. All six raw/traced/saved complete-object triples
are identical. This includes symbols, data, relocations and nonexact bodies.
The debugger runs extracted unchanged image binaries under the host's GDB
and 32-bit runtime; the assembler runs inside the image. This is not a claim
that the debugger uses the original container runtime.

The availability/cursor model validates 1,410 searches and 8,428 candidate
visits in the observed HI/SI general-register domain. Two dynamic positive
pairs, two independently executed equal event streams and four malformed
trace refusal controls establish that the observer and audit actually fire.

| Control | Searches | Candidate visits |
| --- | ---: | ---: |
| Fax retained | 11 | 162 |
| Fax split tail | 12 | 208 |
| Fax retained repeat | 11 | 162 |
| Constructor pre-retype | 459 | 2648 |
| Constructor post-retype | 458 | 2600 |
| Constructor pre-repeat | 459 | 2648 |

EpochDetectV29 enters with cursor2 and selects hard register2 (ECX) in the
baseline, versus cursor3 and register0 (EAX) after the earlier quality-body
control. The historical C2 constructor's first selected scratch likewise
changes from cursor2/ECX to cursor52/EAX. The exact input and allocated
patterns of those targets were already shown equal through27flow2.

The observed allocation order has53 slots and ends with five zero entries.
Those slots repeatedly name EAX; cursor49,50,51,52 and0 are therefore not
equivalent to a uniform EAX/EDX/ECX rotation. Official i386.c's
`x86_order_regs_for_local_alloc` explicitly fills unused slots with zero.
The installed trace measures the actual table and verifies every candidate
against it. A model that assumes a permutation of53 unique registers is wrong.

The constructor writer makes158 versus157 searches. Its complete site sequence
is explained by removing the old +0x434 immediate-SI store search and the
systematic UID shift caused by the retype. That old search is UID1246, entering
with cursor1, selecting ESI and leaving cursor14. The correctly typed SF store
does not request that scratch. The writer's final bytes still agree. This
establishes the missing search and later cursor difference dynamically; it
does not justify restoring the wrong field type to recover constructor bytes.

All1,410 observed enclosing matchers return replacements; none return NULL.
The proposed rejected-match explanation is not observed here. Failure-reset
behavior and other modes/classes are not dynamically established by these
controls. Do not generalize the model to them without positive controls.

Earlier invalid probes remain excluded: an assumed direct matcher caller
was refuted by the actual stack; an assumed `rtwint` field was refuted by the
installed RTL layout (`u.hwint`). Both stop the replay and receive no grades.

```
python3 tools/gentoo_peep2_search_reproduce.py --output build/gentoo-peep2-search-witnesses
python3 tools/gentoo_peep2_search_audit.py
```

Artifacts contain complete commands/versions, preprocessed hashes, input
patterns, event streams and object hashes. This diagnostic recovers a cause
of register-color divergence, not an original source/profile preimage.
