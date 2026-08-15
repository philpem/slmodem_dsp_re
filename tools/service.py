#!/usr/bin/env python3
"""Which service needs each unwritten function: data mode, fax, or neither.

WHY THIS EXISTS

"Leave fax until last" cannot be executed against `worklist.py`'s output,
because that groups by translation-unit SPAN and a span is a bracket over
many TUs.  The one printed `class1tx.c +94` covers 95 of them, and thirteen
symbols inside it -- `FPM_ECC_cancel`, `FPM_SRE_recover`, `FPM_FSE_receive`,
`VTB_decoder` and the rest of the shared DSP -- are what V.32 and V.22 are
built on.  Defer the span and you defer the V.32 echo canceller.

So the partition has to be by REACHABILITY from each service's entry points,
and this computes it.

WHAT ACTUALLY GETS THIS RIGHT, AND THE WRONG ANSWER

Seed each service with ITS OWN entry points and nothing else.  A first
attempt seeded every indirect target as a data entry point -- on the theory
that `closure.py` cannot follow a function pointer in a dispatch table, which
is true -- and reported that data mode needs `V17RX_create`, `V27RX_create`,
`faxvmi_hdlc_unframe` and `TxNextStateV17`.  It does not.  Seeding a table's
ARMS as roots throws away the question of who reaches the TABLE, and the arms
of the fax state machines are fax.

The op-struct targets that ARE service entry points -- `vpcm_create`,
`v32_create`, `b103_create` and the rest -- are named in DATA below instead,
which is what `tools/indirect.py` was written to discover.  That is the whole
fix: name the entry points, do not bulk-seed the tables.

A `data symbol -> the .text it points at` edge was then built and MEASURED
against this, on the assumption it would be needed to reach the modulations.
It changes not one number: `FAX_create` reaches `V17RX_create` by ordinary
calls, through `fax_class1_create`, `FAXVMI_create`, `vxx_create` and
`v17rx_create`.  So it is not here, and this paragraph is why it should not
be added back without a case that measures differently.

WHAT IT DOES NOT KNOW

Run-time reachability, same as `closure.py`.  And it walks the BLOB's graph
with an empty have-set on purpose -- the question is what the ORIGINAL needs
behind an entry point, not what our tree still lacks, so a written function
is walked through rather than stopped at (contrast finding 330, which is
right for the link question and wrong for this one).
"""
import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

ap = argparse.ArgumentParser(
    description="Partition the unwritten symbols by service.")
ap.add_argument("--obj", default=os.environ.get("BLOB",
                                                "../slmodemd/dsplibs.o"),
                help="the blob; also exported as BLOB for closure.py")
ap.add_argument("--tumap", default="build/tumap.json")
ap.add_argument("--list", choices=["data", "fax", "other", "none"],
                help="print the members of one class instead of the summary")
args = ap.parse_args()

# `closure.py` reads its blob from $BLOB AT IMPORT TIME, so --obj has to reach
# the environment before the import or the two halves read different objects.
# readyqueue.py carries the same three lines for the same reason.
os.environ["BLOB"] = args.obj

# `coverage` before `closure`: closure.py strips tools/ from sys.path at
# import time (the `dis.py` shadow fix), after which `import coverage` finds
# the system code-coverage package instead.  See tools/readyqueue.py.
import coverage                                          # noqa: E402
import closure                                           # noqa: E402

FAX = ["FAX_create", "FAX_delete", "FAX_process", "FAX_class1_command"]
DATA = ["vpcm_create", "vpcm_delete", "vpcm_run", "v32_create", "v32_delete",
        "v22_create", "v22_delete", "b103_create", "b103_delete", "v23_create",
        "v23_delete", "dp_wrapper_run", "v8_create", "v8_delete", "v8_process",
        "call_create", "call_delete", "call_run", "dcr_create", "dcr_delete",
        "dcr_process", "dp_runtime_create", "dp_runtime_delete",
        "prop_dp_init", "prop_dp_exit"]
OTHER = ["VOICE_create", "VOICE_delete", "VOICE_process", "VOICE_command",
         "CID_create", "CID_delete", "CID_process", "RD_create", "RD_delete",
         "RD_process", "RD_ring_details"]

# The guard against the mis-seeding above, and it is SHOWN TO FIRE: with the
# indirect targets bulk-seeded as data roots, all four of MUST_BE_FAX land in
# `data` and this exits non-zero.  Finding 134: a check that cannot fail is
# not a check.
MUST_BE_FAX = ["V17RX_create", "V27RX_create", "V29RX_create",
               "faxvmi_hdlc_unframe", "TxNextStateV17"]
MUST_BE_DATA = ["FPM_ECC_cancel", "V32OrgNextState", "v22_originate",
                "_ZN12V90Equalizer7processEPfjPsS0_Rj"]


def main():
    if not os.path.exists(args.obj):
        sys.exit("service.py: no blob at %s.  The default is RELATIVE to the "
                 "tree root." % args.obj)

    syms, sec, edges = closure.build_graph()
    have = closure.ours()

    def reach(roots):
        seen, stack = set(), [r for r in roots if r in syms]
        while stack:
            n = stack.pop()
            if n in seen or n not in syms:
                continue
            seen.add(n)
            stack.extend(edges.get(n, ()))
        return seen

    r_fax, r_data, r_oth = reach(FAX), reach(DATA), reach(OTHER)
    unwritten = {n for n in syms if n not in have
                 and closure.kind_of(syms, sec, n) == "call"}

    groups = {
        "data": unwritten & r_data,
        "fax": (unwritten & r_fax) - r_data - r_oth,
        "other": (unwritten & r_oth) - r_data,
        "none": unwritten - r_data - r_fax - r_oth,
    }

    def where(n):
        for k, s in groups.items():
            if n in s:
                return k
        return "written"

    bad = ([n for n in MUST_BE_FAX if where(n) not in ("fax", "written")] +
           [n for n in MUST_BE_DATA if where(n) not in ("data", "written")])
    if bad:
        print("service.py: THE PARTITION IS WRONG -- these landed in the "
              "wrong class:", file=sys.stderr)
        for n in bad:
            print("   %-40s %s" % (n[:40], where(n)), file=sys.stderr)
        sys.exit("the partition is wrong; do not use these numbers")

    if args.list:
        addr = coverage.blob_addresses(args.obj)
        tus = coverage.load_tus(args.tumap)
        for size, n in sorted(((syms[n][0], n) for n in groups[args.list]),
                              reverse=True):
            print("%7d  %-56s %s"
                  % (size, n[:56],
                     coverage.area_of(addr.get(n, -1), tus, syms[n][1])))
        return

    def B(s):
        return sum(syms[n][0] for n in s)

    print("the unwritten call symbols, by which service needs them")
    print("(each service seeded with its own entry points only -- see the")
    print(" header for why bulk-seeding the dispatch tables is wrong)\n")
    label = {"data": "DATA MODE  V.90/V.92/V.34/V.32/V.22/B.103/V.23/V.8",
             "fax": "FAX only   nothing in data mode reaches it",
             "other": "voice / Caller ID / ring detect only",
             "none": "no entry point reaches it"}
    for k in ("data", "fax", "other", "none"):
        print("  %-52s %4d symbols %7d bytes"
              % (label[k], len(groups[k]), B(groups[k])))
    print("\n  sanity checks passed: %d fax, %d data"
          % (len(MUST_BE_FAX), len(MUST_BE_DATA)))

    addr = coverage.blob_addresses(args.obj)
    tus = coverage.load_tus(args.tumap)
    for k in ("data", "fax"):
        print("\n%s, by span:" % label[k].split()[0])
        per = collections.Counter()
        for n in groups[k]:
            per[coverage.area_of(addr.get(n, -1), tus, syms[n][1])] += \
                syms[n][0]
        for span, b in per.most_common():
            print("   %-30s %8d" % (span, b))


if __name__ == "__main__":
    main()
