#!/usr/bin/env python3
"""
paramcheck.py -- hold V90Parameters.h and V92Parameters.h to the object.

Both headers are LAYOUT ONLY: not one of their members is written yet, so no
differential test in this tree touches them and nothing else can notice if a
later batch moves a field.  That is exactly the shape gates.md calls "a result
indistinguishable from success" -- the suite would stay green while every
`V90Parameters *` in the span pointed at a different set of offsets.

So the blob is the oracle directly.  This re-extracts the (name, offset, type)
map from `V90Parameters::loadParams` and `V92Parameters::loadParams` with
tools/vparse.py, re-reads the `/* +0xNNN */` annotations out of the headers,
and fails on any difference in either direction.  It also re-reads the two
`sysdep_malloc` sizes that bound the objects and checks the last field lands
where they say it does.

Run by `make phase` through the `params` target.

Shown to fire, per gates.md rule 3.  With the tree clean it prints two OK
lines and exits 0; each of these exits 1 and names the field:

    change  +0x008 HW_CODEC_TYPE      -> +0x00c    OFFSET
    change  `int PROBING_MODE`        -> `float`   TYPE
    change  a field's name                         NAME / MISSING
    delete  the last field                         SIZE and MISSING
"""
import importlib.util
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

_spec = importlib.util.spec_from_file_location('vparse',
                                               os.path.join(HERE, 'vparse.py'))
vparse = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(vparse)

# (header, mangled loadParams, sizeof, the two .text addresses whose
# `sysdep_malloc` immediate is that sizeof -- quoted so a reader can check)
CLASSES = (
    ('V90Parameters.h', '_ZN13V90Parameters10loadParamsEPc', 0x558,
     ('0x19551', '0x197b1')),
    ('V92Parameters.h', '_ZN13V92Parameters10loadParamsEPc', 0xdc,
     ('0x13d90', '0x13f20')),
)

# Reproduced original defects that invert the alias rule below.  At
# V90Parameters +0x0f0 the object reads the `BLL_TRN1_QC_SLOW_K1` string and
# then the `BLL_TRN1_QC_SLOW_K2` string into the SAME field; the second read is
# the original's defect (docs/deviations.md D901), so the header correctly
# carries the FIRST read's name.  The entry is (index into the reads that the
# header carries, the OTHER read's name, why), and the gate still requires the
# defect's second read to be present -- `t_v90loadparams` proves the call
# sequence, this proves the field name that sequence cannot.
DEFECT_READS = {
    ('V90Parameters.h', 0x0f0): (0, 'BLL_TRN1_QC_SLOW_K2',
                                  'docs/deviations.md D901'),
}

FIELD_RE = re.compile(
    r'^\t(int|float)\s+(\w+);\s*/\*\s*\+0x([0-9a-f]+)(.*?)\*/', re.M)

# Every annotated member, whatever its type -- the `_tagModemParameters *` at
# +0x000 and the 51 `unnamed_*` slots included.  FIELD_RE deliberately sees
# only the two the blob map can speak about; this one is what the emitted
# offsetof assertions are built from, and it has to cover the whole object.
# Deleting one `unnamed_*` line moves every field after it four bytes while
# every comment stays put, and the text comparison alone cannot see that.
ANY_FIELD_RE = re.compile(
    r'^\t[A-Za-z_][\w\s\*]*?(\w+);\s*/\*\s*\+0x([0-9a-f]+)', re.M)


def from_blob(obj, sym):
    strs = {s: vparse.strings_of(obj, s)
            for s in ('.rodata.str1.1', '.rodata.str1.4')}
    insns = vparse.body(obj, sym)
    best = None
    for cand in range(0x08, 0x60, 4):
        calls = vparse.run(insns, cand)
        n = sum(1 for _, s in calls if s.get(8) and s.get(8)[0] == 'this')
        if best is None or n > best[0]:
            best = (n, calls)
    n, calls = best
    if n != len(calls):
        sys.exit('paramcheck.py: %s -- %d of %d calls unresolved; vparse.py '
                 'cannot read this function and NOTHING below is checked'
                 % (sym, len(calls) - n, len(calls)))
    # Every read of an offset, in call order.  The last read is normally the
    # name the header carries; earlier reads of the same offset are aliases.
    # A reproduced defect can invert that -- see DEFECT_READS above.
    out = {}
    for kind, slots in calls:
        nm, ptr = slots.get(4), slots.get(8)
        out.setdefault(ptr[1], []).append(
            (strs[nm[1]][nm[2]], 'float' if kind.endswith('float') else 'int'))
    return out


def from_header(path):
    out = {}
    for m in FIELD_RE.finditer(open(path).read()):
        out[int(m.group(3), 16)] = (m.group(2), m.group(1))
    return out


def emit(path, header, cls, size, out):
    """
    A translation unit that makes the COMPILER check the annotations.

    The text comparison above holds the header to the object; this holds the
    header to itself, which is the other half and the one `offcheck.py` does
    for every `struct` in the tree.  These are `class`es, so offcheck cannot
    see them: it matches `^struct` and compiles whatever it does not skip as
    C, where `class` is a parse error.

    It matters because the text comparison SKIPS every offset the blob map
    does not cover -- +0x000 and the 51 `unnamed_*` slots `setToDefault`
    writes and `loadParams` never reads.  Delete one `unnamed_*` line and
    every named field after it really moves four bytes while its comment
    stays put, and `paramcheck` alone exits 0.  So does every other gate.

    Guarded on a 32-bit pointer: +0x000 is a `_tagModemParameters *`, so under
    the 64-bit syntax pass every offset after it differs and the file would be
    a wall of failures that mean nothing.  `docs/v90rest.md` says wave 1 paid
    for that guard twice.
    """
    src = open(path).read()
    lines = ['/* GENERATED by tools/paramcheck.py -- do not edit. */',
             '#include "dsplib/%s"' % header,
             '#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4',
             'typedef char pc_size_%s[(sizeof(%s) == %#x) ? 1 : -1];'
             % (cls, cls, size)]
    n = 0
    for m in ANY_FIELD_RE.finditer(src):
        lines.append('typedef char pc_%s_%s[((int)__builtin_offsetof(%s, %s)'
                     ' == %#x) ? 1 : -1];'
                     % (cls, m.group(1), cls, m.group(1),
                        int(m.group(2), 16)))
        n += 1
    lines += ['#endif', 'extern int pc_%s_dummy;' % cls]
    with open(out, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    return n


def main():
    obj = os.environ.get('BLOB') or os.path.join(ROOT, '..', 'slmodemd',
                                                 'dsplibs.o')
    emitdir = None
    if len(sys.argv) > 2 and sys.argv[1] == '--emit':
        emitdir = sys.argv[2]
    bad = 0
    for header, sym, size, where in CLASSES:
        path = os.path.join(ROOT, 'include', 'dsplib', header)
        blob = from_blob(obj, sym)
        hdr = from_header(path)
        n = 0
        for off in sorted(set(blob) | set(hdr)):
            reads, h = blob.get(off), hdr.get(off)
            if not reads:
                continue          # setToDefault-only field; loadParams is
                                  # silent about it and so is this check
            sel = DEFECT_READS.get((header, off))
            if sel:
                idx, other, why = sel
                b = reads[idx]
                if len(reads) != 2 or reads[1 - idx][0] != other:
                    print('  DEFECT   +0x%03x  %s: expected the reproduced '
                          'second read of %s (%s), found %s'
                          % (off, header, other, why,
                             [r[0] for r in reads]))
                    bad += 1
                    continue
            else:
                b = reads[-1]     # the last read is the one that survives
            if h is None:
                print('  MISSING  +0x%03x  %s %s -- in the object, not in %s'
                      % (off, b[1], b[0], header))
                bad += 1
                continue
            if b[0] != h[0]:
                print('  NAME     +0x%03x  object says %s, header says %s'
                      % (off, b[0], h[0]))
                bad += 1
            elif b[1] != h[1]:
                print('  TYPE     +0x%03x  %s: object reads it as %s, header '
                      'declares %s' % (off, b[0], b[1], h[1]))
                bad += 1
            else:
                n += 1
        last = max(hdr) if hdr else -1
        if last + 4 != size:
            print('  SIZE     %s: last field at +0x%03x, so the object ends at '
                  '+0x%03x, but sysdep_malloc at .text+%s and +%s asks for '
                  '0x%x' % (header, last, last + 4, where[0], where[1], size))
            bad += 1
        extra = ''
        if emitdir:
            cls = header[:-2]
            m = emit(path, header, cls, size,
                     os.path.join(emitdir, '_%s.cpp' % cls))
            extra = ', %d offsetof assertions emitted' % m
        print('%-18s %3d fields checked against %s, sizeof 0x%x%s'
              % (header, n, sym, size, extra))
    if bad:
        print('\nparamcheck: %d disagreement(s) between the headers and the '
              'object' % bad)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
