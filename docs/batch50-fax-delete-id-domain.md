# Delete owner reload and frame-ID result controls

902f47fa fixed profile. FAXVMI_delete reloads link/framer owner fields after
each unknown delete/free call in blob; current source snapshots both before
first call. Cross removal of the two independent snapshots. Keep free order,
slot type and every other function unchanged. Reloads matter if callbacks
update the outer owner, as evidenced in the original.

GetT30FrameIDFromBuffer has matching compare/table/mask instructions except
invalid-address return is physically before the valid block rather than a
shared tail. Cross only named invalid/default result with valid nested arm
versus current early reject, and positive valid-address outer guard with
unchanged inner statements. Three nonbaseline forms: positive guard;
default/shared result; default/shared result inside positive guard. No ABI,
flags, padding, declaration permutations or register fits.
