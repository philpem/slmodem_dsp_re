#!/usr/bin/env python3
"""Check the `name .text 0xADDR SIZE` banners in source comments against the object.

WHY THIS EXISTS

Almost every file in `src/` opens with a banner naming what it reconstructs:

    Reconstructed from dsplibs.o:
      V22FP_create   .text 0x087990  2,449 bytes

Those three fields are a claim about the object, and until this tool there was
nothing that read them.  The name is checked implicitly -- the function has to
exist for anything to link -- but the ADDRESS and the SIZE are prose, and prose
is not compiled.

Finding F8535 is what prompted it.  Five task briefs in one wave carried the
same shape, and seven of nine addresses in them were wrong while every size was
right: the sizes had come from a tool that reads the object and the addresses
had been typed.  One of the wrong ones named a real function that was not the
one it claimed, which is the failure mode that looks most like being right.

No work was harmed there, because `tools/dis.py` resolves a SYMBOL NAME through
`nm` and ignores whatever address the reader had in mind.  That is exactly why
a wrong banner can sit in a file indefinitely: nothing consumes it but a human,
and a human reading it has no reason to doubt it.

WHAT IT DOES NOT DO

It says nothing about whether the reconstruction is correct -- that is the
differential tier's job and this tool cannot help with it.  It checks three
numbers against `nm -S`, which is all a banner claims.

A symbol the object does not define at all is reported separately from a
mismatch, because the two mean different things: the first is usually a
static helper this tree invented (legitimate, and it should not carry a
`.text` banner), the second is a stale number.

USAGE

    python3 tools/bannercheck.py                       # the whole tree
    python3 tools/bannercheck.py src/pump/v22          # one directory
    python3 tools/bannercheck.py src/pump/v22/v22fp.c  # one file

Exits non-zero when a banner disagrees with the object.  It also exits non-zero
when it finds NO banners at all, because a checker that silently measured
nothing is the defect findings F2400, F3055 and F3110 are all the same shape of
-- read the denominator it prints, not the absence of complaints.
"""

import os
import re
import subprocess
import sys

# `name .text 0xADDR` and an optional decimal size, with or without commas.
# The trailing "bytes" is not required: the tree spells it both ways.
BANNER = re.compile(
    r'\b([A-Za-z_][A-Za-z0-9_:~]*)\s+\.text\s+0x0*([0-9a-fA-F]+)'
    r'(?:\s+([0-9][0-9,]*))?')

# PROSE MENTIONS AN ADDRESS TOO -- "the block at .text 0x08c340" -- and the
# regex above cannot tell that from a banner, because the only difference is
# that "at" is an English word and `TxNOP` is not.  A symbol in this object
# effectively always carries an underscore, a digit or a capital; a bare
# lower-case word almost never is one.  So a name failing that test is counted
# as prose and SKIPPED -- and the count is printed, because a checker that
# silently drops rows is the dead detector findings F2400 and F3110 are about.
#
# The cost is that a real all-lower-case symbol with no underscore -- `pow` is
# the only plausible one here -- would be skipped as well.  Named so the next
# reader knows the rule rather than guessing at the number.
SYMBOLISH = re.compile(r'[_0-9A-Z]')

SUFFIXES = ('.c', '.cpp', '.h')


def blob_path():
    """The same default `make print-BLOB` resolves, so a worktree works."""
    env = os.environ.get('BLOB')
    if env:
        return env
    try:
        common = subprocess.run(['git', 'rev-parse', '--git-common-dir'],
                                capture_output=True, text=True,
                                check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return 'ref/slmodemd/dsplibs.o'
    return os.path.join(os.path.dirname(os.path.abspath(common)),
                        'ref', 'slmodemd', 'dsplibs.o')


def symbols(obj):
    """name -> [(addr, size)].  A name can appear twice; both are accepted."""
    out = subprocess.run(['nm', '-S', obj], capture_output=True,
                         text=True).stdout
    table = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) == 4:
            table.setdefault(f[3], []).append((int(f[0], 16), int(f[1], 16)))
    return table


def sources(args):
    if not args:
        args = ['src', 'include']
    for arg in args:
        if os.path.isfile(arg):
            yield arg
            continue
        for root, _dirs, files in os.walk(arg):
            # `re/` belongs to a different effort and is not ours to read.
            if os.path.basename(root) == 're':
                _dirs[:] = []
                continue
            for name in sorted(files):
                if name.endswith(SUFFIXES):
                    yield os.path.join(root, name)


def main():
    obj = blob_path()
    if not os.path.exists(obj):
        sys.exit('bannercheck.py: no object at %s (set BLOB=)' % obj)
    table = symbols(obj)
    if not table:
        sys.exit('bannercheck.py: `nm -S %s` named nothing' % obj)

    agree = bad = absent = prose = 0
    for path in sources(sys.argv[1:]):
        try:
            lines = open(path, encoding='utf-8', errors='replace').readlines()
        except OSError:
            continue
        for n, line in enumerate(lines, 1):
            m = BANNER.search(line)
            if not m:
                continue
            name, addr = m.group(1), int(m.group(2), 16)
            size = m.group(3)
            if not SYMBOLISH.search(name):
                prose += 1
                continue
            if name not in table:
                print('  ABSENT  %s:%d  %s is not defined by the object'
                      % (path, n, name))
                absent += 1
                continue
            here = dict(table[name])
            if addr not in here:
                print('  ADDR    %s:%d  %s says 0x%06x, object says %s'
                      % (path, n, name, addr,
                         ', '.join('0x%06x' % a for a in sorted(here))))
                bad += 1
                continue
            if size is not None and int(size.replace(',', '')) != here[addr]:
                print('  SIZE    %s:%d  %s says %s, object says %d'
                      % (path, n, name, size, here[addr]))
                bad += 1
                continue
            agree += 1

    total = agree + bad + absent
    print('bannercheck.py: %d banner(s) read from %s' % (total, obj))
    print('  %d agree, %d disagree, %d name no symbol the object defines'
          % (agree, bad, absent))
    print('  %d further `.text 0x...` mention(s) skipped as prose' % prose)
    if total == 0:
        sys.exit('bannercheck.py: NO BANNERS FOUND, so nothing was checked '
                 'and a clean run means nothing.  Check the paths given.')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
