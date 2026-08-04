#!/usr/bin/env python3
"""
Emit the `objcopy --redefine-syms` map that lets the blob and the
reconstruction coexist in one differential-test binary.

The problem
-----------
Tier-1 testing links dsplibs.o alongside the reconstruction and runs both from
one input, comparing outputs. They export the same 2350 names, so one side
must be renamed. `objcopy --prefix-symbols=ref_` looks like the answer and is
a trap: it renames *undefined* symbols too, so the blob ends up calling
`ref_sysdep_malloc`, which nothing defines, and the link fails.

The obvious fix - un-prefix every undefined symbol - is worse, because it is
silently wrong rather than loudly broken. The blob's 24 imports split into two
very different groups:

  Stateless runtime   (memcpy, sysdep_malloc, __divdi3 ...)
      Safe to share. Both sides calling the same allocator is fine.

  Stateful callbacks  (modem_get_bits, modem_put_bits, modem_get_param ...)
      Catastrophic to share. `modem_get_bits` hands out bits from one stream;
      if both sides draw from it, each consumes the bits the other should have
      seen and every comparison downstream is garbage - while still looking
      like a plausible waveform. This is the single easiest way to get a
      confidently wrong test result.

So the stateful imports are prefixed too, and the harness supplies a `ref_`
shim for each, giving the blob its own independent bit source, parameter store
and datapump registry.

Usage:
    symmap.py <dsplibs.o> [--prefix ref_] [-o symmap.txt]
    objcopy --redefine-syms=symmap.txt dsplibs.o dsplibs_ref.o
"""

import argparse
import subprocess
import sys

# Safe to share between the two implementations: no state that one side
# consuming can perturb what the other observes.
SHARED_IMPORTS = {
    "memcpy", "__divdi3", "__moddi3",
    "sysdep_malloc", "sysdep_free", "sysdep_memcpy", "sysdep_memset",
    "sysdep_strcpy", "sysdep_strcat", "sysdep_strlen",
    "sysdep_sprintf", "sysdep_vsnprintf",
}

# Must NOT be shared - each side needs its own. The harness defines a
# `<prefix><name>` shim for every one of these.
STATEFUL_IMPORTS = {
    "modem_get_bits", "modem_put_bits",
    "modem_get_param", "modem_set_param", "modem_get_sreg",
    "modem_send_to_tty", "modem_recv_from_tty",
    "modem_debug_log_data", "dsplibs_debug_level", "dsplibs_debug_printf",
    "modem_dp_register", "modem_dp_deregister",
}


def nm(obj, *flags):
    out = subprocess.run(["nm", *flags, obj], capture_output=True, text=True,
                         check=True).stdout
    return out.splitlines()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("obj")
    ap.add_argument("--prefix", default="ref_")
    ap.add_argument("-o", "--output")
    ap.add_argument("--globals", metavar="FILE",
                    help="also write the file-local symbols that must be "
                         "globalized before the rename map can apply")
    args = ap.parse_args()

    defined = sorted({ln.split()[-1] for ln
                      in nm(args.obj, "--defined-only", "--extern-only")
                      if len(ln.split()) >= 3})

    #
    # FILE-LOCAL FUNCTIONS ARE ALIASABLE AFTER ALL.
    #
    # This tool, and coverage.py's report, said for a long time that a symbol
    # the object keeps file-local "can never be differentially tested
    # directly, because objcopy cannot rename it".  objcopy can: pass it
    # through --globalize-symbols first and --redefine-syms in a second pass
    # sees an ordinary global.  55 symbols and 18,566 bytes were written off
    # on that basis, 15 of them (6,192 bytes) functions this tree has already
    # reconstructed -- including `call_run`, `b103_process`, `v23_process`
    # and `v8_process`, the top-level entry points of four datapumps, which
    # were reachable only through a caller.
    #
    # Safe here because no local name occurs twice in this object; two static
    # functions of the same name in different translation units would collide
    # the moment both went global, so this checks rather than assumes.
    #
    raw = [ln.split()[-1] for ln in nm(args.obj, "--defined-only")
           if len(ln.split()) >= 3 and ln.split()[-2] in "tdbr"]
    raw = [s for s in raw if s not in defined]
    # Count on the RAW list, before deduplication -- counting a set finds
    # nothing, which is the shape of check that reports clean because it
    # cannot fail.
    seen = {}
    for s in raw:
        seen[s] = seen.get(s, 0) + 1
    dupnames = [s for s, n in seen.items() if n > 1]
    local = sorted(seen)
    # A name used by two translation units cannot be globalized -- both would
    # become the same global and the link would take one at random.  Those
    # stay local and stay untestable-directly, which is the old situation for
    # a few symbols rather than for all of them.
    local = [s for s in local if s not in dupnames]
    defined = sorted(set(defined) | set(local))
    undefined = sorted({ln.split()[-1] for ln in nm(args.obj, "-u")
                        if len(ln.split()) >= 2})

    # Every import must be classified. An unclassified one is a decision
    # nobody made, so fail loudly rather than pick a default.
    unknown = set(undefined) - SHARED_IMPORTS - STATEFUL_IMPORTS
    if unknown:
        sys.exit("error: unclassified imports (add to SHARED_IMPORTS or "
                 "STATEFUL_IMPORTS in %s):\n  %s"
                 % (__file__, "\n  ".join(sorted(unknown))))

    clash = [s for s in defined if s.startswith(args.prefix)]
    if clash:
        sys.exit("error: %d symbols already begin with %r; pick another "
                 "prefix" % (len(clash), args.prefix))

    rename = defined + [s for s in undefined if s in STATEFUL_IMPORTS]

    lines = ["# generated by tools/symmap.py - do not edit",
             "# %d defined symbols + %d stateful imports renamed to %s*"
             % (len(defined), len(rename) - len(defined), args.prefix),
             "# shared (deliberately NOT renamed): %s"
             % " ".join(sorted(SHARED_IMPORTS))]
    lines += ["%s %s%s" % (s, args.prefix, s) for s in rename]
    text = "\n".join(lines) + "\n"

    if args.globals:
        with open(args.globals, "w") as f:
            f.write("".join(s + "\n" for s in local))
        print("wrote %s: %d file-local symbols globalized%s"
              % (args.globals, len(local),
                 ("; %d left local, name used by more than one TU (%s)"
                  % (len(dupnames), " ".join(sorted(dupnames)))) if dupnames else ""))

    if args.output:
        with open(args.output, "w") as f:
            f.write(text)
        print("wrote %s: %d renames (%d defined, %d stateful imports), "
              "%d shared"
              % (args.output, len(rename), len(defined),
                 len(rename) - len(defined), len(SHARED_IMPORTS)))
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
