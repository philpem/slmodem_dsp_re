#!/usr/bin/env python3
"""V34 residual leads from the merged gcc3-candidate-screen (issue #22).

T1 V34GiveProbeResults (ours 105 / blob 71): the screen's best cell
(direct double load + short index) reaches 71 bytes still BYTES44; the
declared next discriminator is the typed owner/root guard.  The blob reads
the receiver pair through an objp+4 base (`lea 0x4(%ecx),%eax; mov
0x248(%eax),%edx; mov 0x24c(%eax),%eax`) -- the documented addressing
artifact (F179/F180, "not evidence of a sub-object at +4").  The owner type
IS reconstructed (struct v34_object: v90_receiver +0x24c, k56flex_receiver
+0x250, probe_results +0xa258), so this is a spelling family, not a header.

T2 VPcmV34GetSNR (ours 90 / blob 99): the divide prefix is byte-identical;
the difference is the old-value (`last`) loop lifetime.  Blob: dedicated
callee-saved %esi, initialised at entry, assigned after the imul via
`mov %ecx,%esi`, copied `mov %esi,%eax` before loop 2; the skip path tests
it un-folked.  Ours: coalesced into %eax (assigned at loop top, imul
in-place on %ecx), skip-path `if (last > 0)` folded to a direct jmp.

All cells value-identical spellings; production sources untouched; scored
only with byteident.py.  No fuzzing or mutation execution.
"""
import playbook_small_patterns as driver

REV = 'a5de26b5'
OUT_NAME = 'playbook-v34-residual'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v34/v34info.c', 'src/pump/v34/VPcmV34Main.cpp')

GUARD_TYPED = ('\t/* Neither PCM receiver running: nothing to record. */\n'
               '\tif (obj->v90_receiver == 0 && obj->k56flex_receiver == 0)\n'
               '\t\treturn 0;\n')
GUARD_PLUS1 = ('\tconst int *recv = (const int *)objp + 1;\n\n'
               '\t/* Neither PCM receiver running: nothing to record. */\n'
               '\tif (recv[0x92] == 0 && recv[0x93] == 0)\n'
               '\t\treturn 0;\n')
GUARD_CHAR4 = ('\tconst char *base = (const char *)objp + 4;\n\n'
               '\t/* Neither PCM receiver running: nothing to record. */\n'
               '\tif (*(const int *)(base + 0x248) == 0 &&\n'
               '\t    *(const int *)(base + 0x24c) == 0)\n'
               '\t\treturn 0;\n')
GUARD_NOBASE = ('\tconst int *recv = (const int *)objp;\n\n'
                '\t/* Neither PCM receiver running: nothing to record. */\n'
                '\tif (recv[0x93] == 0 && recv[0x94] == 0)\n'
                '\t\treturn 0;\n')

DECLS = '\tint db = 0;\n\tint last = 0;\n'
DECLS_SWAPPED = '\tint last = 0;\n\tint db = 0;\n'
LOOP1 = ('\t\t\tfor (;;) {\n'
         '\t\t\t\tlast = v;\n'
         '\t\t\t\tv = (int)((unsigned int)v * 0x1013u) >> 14;\n'
         '\t\t\t\tif (v <= 0)\n'
         '\t\t\t\t\tbreak;\n'
         '\t\t\t\tdb += 6;\n'
         '\t\t\t}\n')
LOOP1_NEXT = ('\t\t\tfor (;;) {\n'
              '\t\t\t\tint next = (int)((unsigned int)v * 0x1013u) >> 14;\n'
              '\n'
              '\t\t\t\tlast = v;\n'
              '\t\t\t\tv = next;\n'
              '\t\t\t\tif (v <= 0)\n'
              '\t\t\t\t\tbreak;\n'
              '\t\t\t\tdb += 6;\n'
              '\t\t\t}\n')
LOOP1_READLAST = ('\t\t\tfor (;;) {\n'
                  '\t\t\t\tlast = v;\n'
                  '\t\t\t\tv = (int)((unsigned int)last * 0x1013u) >> 14;\n'
                  '\t\t\t\tif (v <= 0)\n'
                  '\t\t\t\t\tbreak;\n'
                  '\t\t\t\tdb += 6;\n'
                  '\t\t\t}\n')


def probe_cells(fn):
    begin = fn.index('\t\tunion {')
    stop = fn.index('\n\t\tp += V34_PROBE_STRIDE;', begin)
    staged = fn[begin:stop]
    assert fn.count('\tint i, k;') == 1
    assert 'obj->probe_results[i] = u.d;' in staged
    assert fn.count(GUARD_TYPED) == 1

    def cell(direct, narrow, guard):
        text = fn
        if narrow:
            text = text.replace('\tint i, k;', '\tshort i;\n\tint k;')
        if direct:
            text = text.replace(staged,
                                '\t\tobj->probe_results[i] = *(const double *)p;')
        if guard is not None:
            assert text.count(GUARD_TYPED) == 1
            text = text.replace(GUARD_TYPED, guard)
        return text

    cells = {'baseline': fn,
             'direct-short': cell(True, True, None),
             'recv-plus1': cell(True, True, GUARD_PLUS1),
             'char-base4': cell(True, True, GUARD_CHAR4),
             'recv-nobase': cell(True, True, GUARD_NOBASE)}
    assert len(cells) == len(set(cells.values())) == 5
    return cells


def snr_cells(fn):
    assert fn.count(DECLS) == 1
    assert fn.count(LOOP1) == 1
    cells = {'baseline': fn,
             'last-first': fn.replace(DECLS, DECLS_SWAPPED),
             'next-temp': fn.replace(LOOP1, LOOP1_NEXT),
             'next-temp-last-first': fn.replace(LOOP1, LOOP1_NEXT).replace(
                 DECLS, DECLS_SWAPPED),
             'mult-reads-last': fn.replace(LOOP1, LOOP1_READLAST)}
    assert len(cells) == len(set(cells.values())) == 5
    return cells


def variants(path, source):
    start, end, fn = driver.function(source, 'V34GiveProbeResults'
                                     if path.endswith('v34info.c')
                                     else 'VPcmV34GetSNR')
    cells = probe_cells(fn) if path.endswith('v34info.c') else snr_cells(fn)
    return {label: source[:start] + text + source[end:]
            for label, text in cells.items()}


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
