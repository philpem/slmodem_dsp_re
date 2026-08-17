#!/usr/bin/env python3
"""
The C++ class structure the blob's mangled names already contain.

WHY THIS EXISTS

886 of dsplibs.o's symbols are Itanium-mangled, across 73 classes and 55 free
functions -- essentially the whole of `VPcmV34Main.cpp` and the float DSP
support beneath it.  Every one of those names encodes the class, the method,
and every parameter type in order, with the author's own enum and struct
names.  For the C half of this object all of that has to come out of the
disassembly, an argument at a time; here it is already written down.

So this reads it out and prints the declarations, which is a header sketch to
start from rather than a thing to guess at.

WHAT MANGLING GIVES, AND WHAT IT DOES NOT

Given, and exact:

    the class name, including template arguments
    the method name, and whether it is a constructor or destructor
    every parameter type, in order, with pointer/reference/const qualifiers
    whether the method itself is const (the `K` before the name)
    the names of enums and structs used as parameters, which are otherwise
      unrecoverable -- `PcmType`, `Phase3ModulatorState`, `tagV90DILdescriptor`

NOT given, and this matters:

    RETURN TYPES.  Itanium mangling omits them for ordinary functions; only
      function templates encode one.  Every `?` in the output below is a
      return type that has to come from the disassembly.
    MEMBER VARIABLES.  Nothing about layout or size.  Bound those the way
      finding 215 did, from the largest `this`-relative displacement the
      methods use.
    BASE CLASSES, access specifiers, and anything `private`.
    Members the compiler inlined everywhere, which have no symbol at all.

So the output is a skeleton with the flesh missing in known places, not a
header.  Treat an unfilled return type as an open question, never as `void`.

CONSTRUCTOR AND DESTRUCTOR VARIANTS

The Itanium ABI's `C<n>` tags are the complete-object and base-object
constructors, and its `D<n>` tags the deleting, complete and base
destructors.  GCC emits more than one for the
same source declaration, which is why `nm` shows what looks like a duplicate
at the same address and the same size.  They are folded here, and the variants
seen are noted, because a class with a deleting destructor had a virtual
one, and this
object is otherwise built `-fno-rtti -fno-exceptions`.

USAGE

    tools/cppstruct.py                       every class, one line each
    tools/cppstruct.py V90Phase3Modulator    full declarations for one class
    tools/cppstruct.py --free                the 55 free functions
    tools/cppstruct.py --types V90PreFilter  types this class needs declared
    tools/cppstruct.py --missing             only what src/ has not defined
"""

import argparse
import collections
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import objtree                                            # noqa: E402

BLOB = os.environ.get("BLOB", "ref/slmodemd/dsplibs.o")


def nm_mangled(path):
    """[(name, size, kind)] for every mangled symbol the object defines."""
    out = subprocess.run(["nm", "-S", "--defined-only", path],
                         capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        f = line.split()
        if len(f) == 4 and f[3].startswith("_Z"):
            syms.append((f[3], int(f[1], 16), f[2]))
        elif len(f) == 3 and f[2].startswith("_Z"):
            syms.append((f[2], 0, f[1]))
    return syms


def demangle(names):
    if not names:
        return {}
    out = subprocess.run(["c++filt"], input="\n".join(names),
                         capture_output=True, text=True).stdout.splitlines()
    return dict(zip(names, out))


def ours():
    """Mangled names our own build already defines.

    REFUSES on an empty object tree rather than returning an empty set: with
    one, every `--missing` column equalled its `members` column and the report
    read as 73 classes with not one method written, at exit 0.  Findings 3055
    and 3110; tools/objtree.py.
    """
    _d, objs = objtree.read("which members src/ already defines")
    found = set()
    for o in objs:
        out = subprocess.run(["nm", "--defined-only", o],
                             capture_output=True, text=True).stdout
        for line in out.splitlines():
            f = line.split()
            if f and f[-1].startswith("_Z"):
                found.add(f[-1])
    return found


#
# Splitting `Class::method(args)` out of a demangled name.
#
# Not by `split("::")`: a parameter can itself be a qualified name, and a
# template argument can contain both `::` and `(`.  Match the qualified name
# that precedes the FIRST parenthesis instead, allowing balanced `<>` inside
# it, and take the last `::` in that.
#
QUAL = re.compile(r"^(?:[\w\s*&]+?\s)??"          # optional leading return type
                  r"((?:[A-Za-z_]\w*(?:<[^()]*>)?::)+)"
                  r"(~?[A-Za-z_]\w*(?:<[^()]*>)?|operator[^(]*)"
                  r"\((.*)\)\s*(const)?\s*$")


def parse(dem):
    """(class, member, args, is_const) or None for a free function."""
    m = QUAL.match(dem)
    if not m:
        return None
    scope = m.group(1)[:-2]
    return scope, m.group(2), m.group(3), bool(m.group(4))


def variant(mangled):
    """The ABI ctor/dtor tag if this symbol is one, else None.

    The tag sits directly after the class NAME and before the parameter list
    -- `_ZN18V90Phase3ModulatorC1EP13V90Parametersj` -- so it follows a
    letter, not the length prefix.  Two earlier versions of this anchored on
    the end of the string and on a preceding digit; both matched nothing and
    printed `None` for every constructor in the object.
    """
    m = re.search(r"(C[123]|D[012])E", mangled)
    return m.group(1) if m else None


def collect():
    syms = nm_mangled(BLOB)
    dem = demangle([s for s, _, _ in syms])
    have = ours()
    classes = collections.defaultdict(dict)
    free = []
    for name, size, kind in syms:
        d = dem.get(name, name)
        #
        # A demangled name with no parenthesis is DATA, not a function: a
        # static member like `V90PreFilter::preFilterCoefType3` (4,960 bytes
        # of coefficients) or a vtable/typeinfo symbol.  An earlier version
        # sent these to the free-function list, where a 4,960-byte
        # coefficient table read as the second largest "function" in the
        # object.
        #
        if "(" not in d:
            rec = dict(mangled=name, size=size, kind=kind, dem=d,
                       have=name in have, var=None, data=True)
            if "::" in d:
                cls, _, member = d.rpartition("::")
                rec.update(cls=cls, member=member, args="", const=False)
                classes[cls][(member, "", False)] = rec
            else:
                free.append(rec)
            continue
        p = parse(d)
        rec = dict(mangled=name, size=size, kind=kind, dem=d,
                   have=name in have, var=variant(name), data=False)
        if p is None:
            free.append(rec)
            continue
        cls, member, args, is_const = p
        rec.update(cls=cls, member=member, args=args, const=is_const)
        # Fold the constructor and destructor variants onto one line.
        key = (member, args, is_const)
        if key in classes[cls]:
            classes[cls][key]["var"] = "%s,%s" % (classes[cls][key]["var"],
                                                  rec["var"])
            classes[cls][key]["have"] |= rec["have"]
        else:
            classes[cls][key] = rec
    return classes, free


#
# Types a class's signatures mention that are not built in.  These are what
# has to be declared -- as an enum, a struct or a forward declaration --
# before the class will compile, and the mangling is the only place several
# of them are named at all.
#
BUILTIN = {"void", "bool", "char", "signed char", "unsigned char", "short",
           "unsigned short", "int", "unsigned int", "long", "unsigned long",
           "long long", "unsigned long long", "float", "double",
           "long double", "wchar_t", "unsigned", "signed", ""}


def types_of(args):
    out = set()
    for a in re.split(r",(?![^<]*>)", args):
        a = a.strip().rstrip("*&").strip()
        a = re.sub(r"^(const|volatile)\s+", "", a).strip()
        a = re.sub(r"\s*(const|volatile)$", "", a).strip()
        if a and a not in BUILTIN and not a.isdigit():
            out.add(a)
    return out


def decl(rec, cls):
    """One declaration line.

    Deliberately emits `?` for an unknown return type rather than `void`: it
    does not compile, so a skeleton cannot be pasted in and quietly built with
    every return silently wrong.
    """
    if rec.get("data"):
        return "        static ? %-55s // %d bytes%s" % (
            rec["member"] + ";", rec["size"],
            "" if rec["have"] else ", NOT WRITTEN")
    ctor = rec["member"] == cls
    dtor = rec["member"].startswith("~")
    ret = "" if (ctor or dtor) else "? "
    body = "%s%s(%s)%s;" % (ret, rec["member"], rec["args"],
                            " const" if rec["const"] else "")
    note = "%d bytes" % rec["size"]
    if rec["var"]:
        note += ", %s" % rec["var"]
    if not rec["have"]:
        note += ", NOT WRITTEN"
    return "        %-64s // %s" % (body, note)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cls", nargs="?", help="one class to expand")
    ap.add_argument("--free", action="store_true", help="free functions only")
    ap.add_argument("--types", metavar="CLASS",
                    help="types this class's signatures require")
    ap.add_argument("--missing", action="store_true",
                    help="omit members src/ already defines")
    args = ap.parse_args()

    classes, free = collect()

    if args.types:
        if args.types not in classes:
            sys.exit("no such class: %s" % args.types)
        need = set()
        for rec in classes[args.types].values():
            need |= types_of(rec["args"])
        print("%s needs these declared (mangling names them; nothing else "
              "does):" % args.types)
        for t in sorted(need):
            kind = "class " if t in classes else "      "
            print("   %s%s" % (kind, t))
        return 0

    if args.free:
        print("Free functions: %d.  `*` = not in src/.  Size is the blob's.\n"
              % len(free))
        for rec in sorted(free, key=lambda r: -r["size"]):
            if args.missing and rec["have"]:
                continue
            print(" %s %-62s %6d" % (" " if rec["have"] else "*",
                                     rec["dem"] + ";", rec["size"]))
        return 0

    if args.cls:
        if args.cls not in classes:
            sys.exit("no such class: %s\nTry: tools/cppstruct.py" % args.cls)
        members = classes[args.cls]
        total = sum(r["size"] for r in members.values())
        print("/*\n * %s -- %d members, %d bytes in the blob.\n"
              " *\n"
              " * `?` is a return type the mangling does not encode; read it\n"
              " * from the disassembly.  No member variables, base classes or\n"
              " * access specifiers are recoverable -- see tools/cppstruct.py.\n"
              " */"
              % (args.cls, len(members), total))
        print("class %s {" % args.cls)
        for key in sorted(members, key=lambda k: (-members[k]["size"], k)):
            rec = members[key]
            if args.missing and rec["have"]:
                continue
            print(decl(rec, args.cls))
        print("};")
        need = set()
        for rec in members.values():
            need |= types_of(rec["args"])
        if need:
            print("\n/* needs declaring: %s */" % ", ".join(sorted(need)))
        return 0

    print("%d classes, %d free functions, from %s.\n"
          "`missing` counts members src/ does not define.\n"
          % (len(classes), len(free), BLOB))
    print("  %-34s %7s %7s %8s" % ("class", "members", "missing", "bytes"))
    rows = []
    for cls, members in classes.items():
        miss = sum(1 for r in members.values() if not r["have"])
        rows.append((sum(r["size"] for r in members.values()), cls,
                     len(members), miss))
    for size, cls, n, miss in sorted(rows, reverse=True):
        print("  %-34s %7d %7d %8d" % (cls, n, miss, size))
    return 0


if __name__ == "__main__":
    sys.exit(main())
