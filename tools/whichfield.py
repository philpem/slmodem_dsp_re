#!/usr/bin/env python3
"""
Which field of a struct lives at byte offset N?

WHY THIS EXISTS

`diff_eq_obj` reports where two objects diverge as a byte offset, because
that is what a byte comparison knows.  An offset is not a diagnosis: it has
to be turned into a field before it says anything about the code, and doing
that by hand means counting shorts down a struct declaration -- which is
exactly the arithmetic that produced findings F46 and F122's misreadings.

Our own objects are built `-g`, so the answer is already in the tree.  This
reads it out.  It resolves through nested structs and arrays, so an offset
inside an embedded object comes back as a path:

    $ tools/whichfield.py struct v34_decision 42
    struct v34_decision + 42  ->  dp.point            (short, +42)

and an offset that lands mid-field says so, because a divergence starting at
the second byte of a short is a different bug from one starting at the first:

    $ tools/whichfield.py struct v34_decision 43
    struct v34_decision + 43  ->  dp.point +1 of 2    (short, +42)

USAGE

    tools/whichfield.py <type...> <offset>       one offset
    tools/whichfield.py <type...> --layout       the whole struct

`<type>` is written as it appears in C -- `struct v34_decision`, or just
`v34_decision` for a typedef.  The type must be one some object under
build/ actually uses; a struct no compiled code mentions is not in the
DWARF, and this says so rather than guessing.
"""

import glob
import os
import sys

#
# `tools/dis.py` shadows the standard library's `dis`, and `inspect` imports
# that on the way up from pyelftools -- so importing anything substantial from
# a script run out of this directory dies with
#
#     AttributeError: module 'dis' has no attribute 'COMPILER_FLAG_NAMES'
#
# which names neither this directory nor the file responsible.  Drop our own
# directory from the search path before the import rather than renaming
# dis.py, whose name is right and which is referenced from several findings.
#
_here = os.path.dirname(os.path.abspath(__file__))
sys.path[:] = [p for p in sys.path if os.path.abspath(p or ".") != _here]

try:
    from elftools.elf.elffile import ELFFile
except ImportError:
    sys.exit("needs pyelftools:  pip install pyelftools")


#
# The LINKED test binaries, not the .o files.  Both carry the DWARF, but a
# relocatable object needs its debug sections relocated to be read, and
# pyelftools refuses on R_386_GOTOFF ("Unsupported relocation type: 9") --
# which our -fno-pie objects do contain.  A linked binary needs no relocation
# and every type any test uses is in one of them.
#
def dies(pattern="build/test/t_*"):
    """Every DIE in every test binary we have built, with its CU."""
    for path in sorted(glob.glob(pattern)):
        if path.endswith(".o") or not os.path.isfile(path):
            continue
        try:
            with open(path, "rb") as f:
                elf = ELFFile(f)
                if not elf.has_dwarf_info():
                    continue
                for cu in elf.get_dwarf_info().iter_CUs():
                    for die in cu.iter_DIEs():
                        yield die, cu
        except Exception:
            continue                    # a binary we cannot read is not fatal


def name(die):
    n = die.attributes.get("DW_AT_name")
    return n.value.decode() if n else None


def typename(die, cu):
    """A readable C type name for a DIE, including the unnamed kinds."""
    if die is None:
        return "?"
    n = name(die)
    if n and die.tag != "DW_TAG_pointer_type":
        return ("struct " + n) if die.tag == "DW_TAG_structure_type" else n
    ref = die.attributes.get("DW_AT_type")
    inner = (cu.get_DIE_from_refaddr(ref.value + cu.cu_offset)
             if ref else None)
    if die.tag == "DW_TAG_pointer_type":
        return (typename(inner, cu) if inner else "void") + " *"
    if die.tag == "DW_TAG_array_type":
        return (typename(inner, cu) if inner else "?") + "[]"
    if die.tag in ("DW_TAG_const_type", "DW_TAG_volatile_type"):
        return typename(inner, cu) if inner else "?"
    return n or die.tag.replace("DW_TAG_", "")


def find_type(want):
    """The DIE for `want`, which may be 'struct foo', 'foo', or a typedef."""
    tag = None
    for kw, t in (("struct ", "DW_TAG_structure_type"),
                  ("union ", "DW_TAG_union_type"),
                  ("class ", "DW_TAG_class_type")):
        if want.startswith(kw):
            tag, want = t, want[len(kw):]
    for die, cu in dies():
        if name(die) != want:
            continue
        if tag and die.tag == tag:
            return die, cu
        # DW_TAG_class_type is here because the C++ half of the object is
        # classes, and `diff_eq_obj(..., V90Jd, ...)` stringifies the bare
        # name -- so the lookup has to answer to `V90Jd` with no keyword.
        if not tag and die.tag in ("DW_TAG_structure_type",
                                   "DW_TAG_union_type", "DW_TAG_class_type",
                                   "DW_TAG_typedef"):
            return die, cu
    return None, None


def resolve(die, cu):
    """Follow typedefs, const and volatile down to the thing itself."""
    while die is not None and die.tag in ("DW_TAG_typedef",
                                          "DW_TAG_const_type",
                                          "DW_TAG_volatile_type"):
        ref = die.attributes.get("DW_AT_type")
        if ref is None:
            return None
        die = cu.get_DIE_from_refaddr(ref.value + cu.cu_offset)
    return die


def size(die, cu):
    d = resolve(die, cu)
    if d is None:
        return 0
    s = d.attributes.get("DW_AT_byte_size")
    if s:
        return s.value
    if d.tag == "DW_TAG_array_type":                 # size is count x element
        elem = resolve(d.attributes.get("DW_AT_type") and
                       cu.get_DIE_from_refaddr(
                           d.attributes["DW_AT_type"].value + cu.cu_offset), cu)
        n = 0
        for sub in d.iter_children():
            c = sub.attributes.get("DW_AT_upper_bound")
            if c is not None:
                n = c.value + 1
        return n * (size(elem, cu) if elem else 0)
    return 0


def walk(die, cu, off, path=""):
    """Deepest field path containing `off`, and where that field starts."""
    d = resolve(die, cu)
    if d is None:
        return path, off, None
    if d.tag == "DW_TAG_array_type":
        ref = d.attributes.get("DW_AT_type")
        elem = cu.get_DIE_from_refaddr(ref.value + cu.cu_offset) if ref else None
        es = size(elem, cu) or 1
        i, rem = off // es, off % es
        return walk(elem, cu, rem, "%s[%d]" % (path, i))
    if d.tag not in ("DW_TAG_structure_type", "DW_TAG_union_type",
                     "DW_TAG_class_type"):
        return path, off, d
    for m in d.iter_children():
        if m.tag != "DW_TAG_member":
            continue
        loc = m.attributes.get("DW_AT_data_member_location")
        base = loc.value if loc else 0
        ref = m.attributes.get("DW_AT_type")
        mt = cu.get_DIE_from_refaddr(ref.value + cu.cu_offset) if ref else None
        ms = size(mt, cu) or 1
        if base <= off < base + ms:
            sub = "%s.%s" % (path, name(m)) if path else name(m)
            return walk(mt, cu, off - base, sub)
    return path, off, d                        # padding, or past the end


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    layout = "--layout" in a
    a = [x for x in a if x != "--layout"]
    if not layout and len(a) < 2:
        sys.exit(__doc__)
    off = 0 if layout else int(a[-1], 0)
    want = " ".join(a if layout else a[:-1])

    die, cu = find_type(want)
    if die is None:
        sys.exit("no DWARF for %r under build/ -- is it a type some compiled\n"
                 "object actually uses?  Build first, or check the spelling\n"
                 "(`struct foo` vs a typedef name)." % want)

    total = size(die, cu)
    if layout:
        print("%s: %d bytes" % (want, total))
        for o in range(total):
            path, rem, leaf = walk(die, cu, o)
            if rem == 0:
                print("  +%-5d %-34s %s" % (o, path or "?",
                                            typename(leaf, cu)))
        return 0

    if off >= total:
        print("%s + %d  ->  past the end (%s is %d bytes)"
              % (want, off, want, total))
        return 0
    path, rem, leaf = walk(die, cu, off)
    tname = typename(leaf, cu)
    lead = "%s + %d  ->  %s" % (want, off, path or "?")
    if rem:
        lead += " +%d of %d" % (rem, size(leaf, cu) if leaf else 0)
    print("%-58s (%s, +%d)" % (lead, tname, off - rem))
    return 0


if __name__ == "__main__":
    sys.exit(main())
