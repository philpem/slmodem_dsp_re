# Completed fax source refinement at 902f47fa

This is comparison of completed source, not a new reconstruction phase.
The fresh 300-TU baseline has 973 exact names. The owned <=650-byte screen
compares 456 functions: 207 exact, 249 nonexact. Eight predeclared domains
compile 83 valid full-TU cells with retained Gentoo 3.4.2-r2 flags,
DSPLIB_REPRODUCE_BUGS, actual compiler/assembler identity and RTL dumps.
Every baseline reproduces its retained object raw. The initial Docker
permission failure and failed V21 source extraction assertion are invalid
preflights, excluded from the 83 successful compilation denominator.

Sixteen original functions, 1,212 blob bytes, become byte-exact:

| Boundary | Exact functions | Bytes |
|---|---|---:|
| Full-width masked index, valid arm first | v17/v21/v27/v29 rx_message and tx_message | 8 x 39 |
| Clear child flag, then guarded one | V17RX_control | 131 |
| Request and child reload after stores | V27RX_control | 125 |
| Nonempty FIFO arm first and child read after status store | TxHdxIdleV17, TxHdxIdleV29 | 2 x 116 |
| Unsigned saved budget | TxHdxIdleV27 | 116 |
| Existing union byte flag lvalue | RxHdxErrorV29, RxHdxIdleV29, RxHdxStartV29 | 59 + 132 + 105 |

The message mask and branch orientation controls each miss independently;
only their crossing reproduces the blob's full-width AND255 and branch
placement. Historical inclusive guard values stay unchanged, including the
out-of-table edge. Message strings/tables and all neighbours stay unchanged.

V17RX_control's boolean assignment had a different read/store boundary:
the blob clears the child first, then samples request bit4 and conditionally
sets it. This can matter under overlap; byte exactness settles the original
boundary. V27 separately needs both uncached request-byte reads after child
writes and conditional child loads in each mask arm; either alone misses.
V21/V29 clear/set transfer misses and is not adopted.

V17/V29 transmitter idle states independently need both source arm order and
the child pointer read after the status-byte store; either alone misses.
V27's already correct CFG instead has a saved short budget whose MOVSWL and
second conversion are absent in the blob. An unsigned short local closes it
without changing the public budget pointer, call or post-call budget reread.

V29 receiver flag updates use ORB/ANDB at +0x19 in the object. Updating the
existing union's flags byte with the original named mask shifted eight bits
closes Error, Idle and Start. Individual and combined three-winner controls
all retain the exact neighbour, same exports, relocation target multiplicity
and named/allocated data. Data/Prtcol/EpochDet/NextState and constructor byte
controls miss; no source from those cells is adopted.

# Closed negative domains

* V17/V29 TX status explicit first/final flags value: six nonbaseline controls
  miss. Final sampling order transfers F10231, but the dead first value still
  folds to memory AND, unlike V27's surviving value. Do not widen with arbitrary
  local types/positions without a new original source boundary.
* SMCv17_init direct default memcpy x observed clear order: three controls
  miss (SIZE1). GenEQTrnSequenceV29 base-owner final access x postdecrement:
  three miss (SIZE1). No scalar/local permutations adopted.
* FCS postdecrement closes size but leaves BYTES28; narrowing polynomial term
  before final nibble OR changes no raw body. Three combined controls miss.
  FIFO_write reversed min owner x postdecrement: three miss, SIZE3 unchanged.
* null_process original signed word comparison x positive-count owner guard:
  three controls miss. Signed-use closes size to BYTES14; guarded signed form
  is BYTES21. No public pointer type change, and no source adopted.
* _rx_look_carrier_init return-assignment zero owner reproduces the original
  source object raw; redundant XOR remains compiler-side, no adoption.
* _hdlc_emulate_receive_state authentic absent level>1 timestamp diagnostic
  x member-owned read index: three controls miss. Combined closes size452 but
  leaves BYTES352. Only added diagnostic string/relocations are the exact
  original format and debug symbols. No source adopted or unrelated CFG fits.

Eight 62-byte process adapters are grade1 exact, BYTES9, and their canonical
bodies stay unchanged before/after the eight message wins. They offer a
cluster for GCC scratch allocation/cursor diagnosis, not source permutations.

# Reproduction and audit

Run the eight batch50_fax_*.py reproductions with their corresponding domain
file via --domain, then tools/batch50_fax_full_audit.py. The full audit records
83 cells, all canonical named data/type/binding/visibility equal, complete
bystander checks, zero exact losses, and original HDLC diagnostic additions.
The declined NextState cell changes jump-table case offsets because its
case instruction widths change; those negative relocation changes are
recorded, not suppressed in an adopted candidate. Winning cells have no
nontext/data changes. Matrices, complete commands/hashes/RTL and audit are
under build/batch50-fax-*/; consolidated report is
build/batch50-fax-full-audit.json. Root performs the final integrated byte
census and batch phase gate before committing.
