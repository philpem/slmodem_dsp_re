# Remaining C++ deletion boundary audit

F11628. At b09cadf7, screen72 production C++ objects containing976 emitted
function occurrences against the retained915-function exact set. Four nonexact
D0/D1/D2/Delete occurrences remain by name: V90Demodulator D1/D2 (already
closed), K56FLEX_Delete, and GenericIIR<float,double> D2. The latter is not
present in the blob, leaving three blob-common occurrences. This selected
symbol-name screen does not cover every possible hidden cleanup method.

K56FLEX_Delete17B versus23B blob at0x102c0 tests null and tail-jumps to free;
the blob calls free0x102ce then returns. Its old C-language exclusion is stale:
F11362 establishes NoK56Flex.cpp provenance. That enables C++ hypotheses but
does not independently establish scalar or array lifetime. No delete cast,
new construction or invented class padding is adopted to suppress a tail call.

A two-caller audit establishes actual factory-pointer use. vpcm_create calls
K56FLEX_Create0x3b26 and stores EAX into root+0xac44 at0x3b2b; it passes
root+0x2c into VPcmV34Create0x3c70. The latter captures subobject+0xac18
at0xaa92 and restores it0xaaf9 after clearing. These refer to the same allocation.
It writes a word at allocation+0xc at0xac67/0xafc6/0xb0a5/0xb320/0xb35a,
reads it0xb32f, and passes the pointer to externalReset0xb350. Another path
loads it0xae46 and calls setMinMaxRates0xb317. The allocation is therefore a
used record, not an untouched placeholder. Factory/delete header statements
about those two leaf bodies do not describe all caller accesses.

Both mangled class methods are empty. Neither caller establishes observable
construction or a nonempty class-member ownership boundary; absent empty
constructor calls could equally reflect inlining. API receiver use and a
record at least16 bytes long do not recover sizeof or a scalar class lifetime.
The original author might have used class delete, but these observations alone
cannot select it uniquely. Primitive-array delete is likewise unsupported.
A future discriminator needs a nonempty owner/member operation or constructor
initialization on this actual allocation. No new source experiment recommended.

Existing t_v92alloc tests live20-byte allocation, null and unknown-pointer
allocator probes. The last are synthetic, not constructed class lifecycles.
This audit runs no fixture, fuzzing or mutation and adopts no source change.
Independent parent inventory reproduces72/976 and the three-common/four-emitted
candidate split. Do not interpret this bounded negative screen as a global
byte-exactness ceiling.
