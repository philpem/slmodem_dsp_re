#!/usr/bin/env python3
"""Queue<float>::write(float) prologue-form family + getSegmentPointer pool family.

Family A (`_ZN5QueueIfE5writeEf`, 71 ours / 85 blob, dI 0): the whole 14-byte
gap is the prologue/epilogue FORM -- ours push/pop (9 bytes), the blob's
sub+mov-save / mov-restore+add (25 bytes) -- the GCC 3.4.2 "fast prologue"
decision (i386.c ix86_compute_frame_layout: mov-saves iff
TARGET_PROLOGUE_USING_MOVE && !expensive_function_p((nregs-1)*20), evaluated
on the reload-time RTL).  The body itself is byte-identical.  Cells are
same-body spellings of the member via a HEADER OVERLAY of dsplib/Queue.h
(candidate-only; production hashes stay authoritative); each must keep every
function size and global identical or the driver rejects the cell.

Family B (`_Z17getSegmentPointer7PcmTypei`, 115/115): grade-0 UNRESOLVED(1) --
every byte equal outside the masked relocation; grade-1 rejects ONLY on the
anonymous .rodata pool addend (blob 2944 = merged cumulative offset; ours 0 =
the TU's sole .rodata content).  No same-shape spelling can move a TU-relative
addend, so the driver runs the baseline reproduction only; the static-table
shape probes run outside the driver as explicitly invalid artifacts.

No fuzzing or mutation execution; static anchor checks and period compiles
only.
"""
import playbook_small_patterns as driver

REV = 'c1617cf8'
OUT_NAME = 'playbook-queue-segptr'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V92Modulator.cpp',
                'src/pump/v90/V90DilDescriptorSettings.cpp')
OVERLAY_SOURCE = 'src/pump/v90/V92Modulator.cpp'
OVERLAY_HEADER = 'dsplib/Queue.h'

WRITE_MEMBER = """\
template <class T>
int Queue<T>::write(T v)
{
\tif (size - count() - 1 == 0)
\t\treturn -1;

\t*wr = v;
\twr = (wr == last) ? buf : wr + 1;
\treturn 0;
}
"""

BASE_HEADER = None  # filled from REV in main()

WRITE_CELLS = {
    'isfull': """\
template <class T>
int Queue<T>::write(T v)
{
\tif (isFull())
\t\treturn -1;

\t*wr = v;
\twr = (wr == last) ? buf : wr + 1;
\treturn 0;
}
""",
    'ifelse': """\
template <class T>
int Queue<T>::write(T v)
{
\tif (size - count() - 1 == 0)
\t\treturn -1;

\t*wr = v;
\tif (wr == last)
\t\twr = buf;
\telse
\t\twr = wr + 1;
\treturn 0;
}
""",
    'opencoded': """\
template <class T>
int Queue<T>::write(T v)
{
\tif (size - (unsigned)((wr + size) - rd) % size - 1 == 0)
\t\treturn -1;

\t*wr = v;
\twr = (wr == last) ? buf : wr + 1;
\treturn 0;
}
""",
    'spacelocal': """\
template <class T>
int Queue<T>::write(T v)
{
\tunsigned int space = size - count() - 1;

\tif (space == 0)
\t\treturn -1;

\t*wr = v;
\twr = (wr == last) ? buf : wr + 1;
\treturn 0;
}
""",
    'wlocal': """\
template <class T>
int Queue<T>::write(T v)
{
\tT *w = wr;

\tif (size - count() - 1 == 0)
\t\treturn -1;

\t*w = v;
\twr = (w == last) ? buf : w + 1;
\treturn 0;
}
""",
}


def header_with_member(member_text):
    assert BASE_HEADER.count(WRITE_MEMBER) == 1
    return BASE_HEADER.replace(WRITE_MEMBER, member_text)


def variants(path, source):
    if path == OVERLAY_SOURCE:
        cells = {'baseline': source}
        for label in WRITE_CELLS:
            cells[label] = source
        return cells
    return {'baseline': source}


def header_overlays(path, label):
    if path == OVERLAY_SOURCE and label in WRITE_CELLS:
        return {OVERLAY_HEADER: header_with_member(WRITE_CELLS[label])}
    return {}


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    import subprocess
    BASE_HEADER = subprocess.check_output(
        ['git', 'show', REV + ':include/dsplib/Queue.h'],
        cwd=driver.ROOT, text=True)
    driver.HEADER_OVERLAYS = header_overlays
    driver.main()
