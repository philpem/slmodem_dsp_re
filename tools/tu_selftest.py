#!/usr/bin/env python3
"""ELF-backed regression cases for TU attribution and ordering policy.

Host binutils constructs fixtures only; no reconstruction objects are built.
Equal local-name sets (including equal types and sizes) deliberately occur in
two inputs.  Their identities come from FILE/symtab occurrences, not a set.
"""
import contextlib
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

# unittest -> inspect must import stdlib dis, not tools/dis.py.
_path = sys.path[:]
try:
    sys.path[:] = [p for p in sys.path
                   if Path(p or ".").resolve() != Path(__file__).resolve().parent]
    import unittest
finally:
    sys.path[:] = _path

import elfinfo
import tumap
import tuattrib
from toolchain import recoverorder


def assemble(directory, stem, filename, body):
    path = directory / (stem + ".o")
    subprocess.run(["as", "--32", "-o", str(path)],
                   input='.file "%s"\n.text\n%s' % (filename, body),
                   text=True, check=True, capture_output=True)
    return path


def function(name, binding="local", size=1):
    return (".%s %s\n.type %s,@function\n%s:\n.space %d,0x90\n"
            ".size %s,.-%s\n" % (binding, name, name, name, size, name, name))


class AttributionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="tu-attribution-")
        cls.directory = Path(cls.temp.name)
        cls.paths = []
        # Same complete local-name/type/section/size sets, different FILEs.
        for stem in ("a", "b"):
            cls.paths.append(assemble(cls.directory, stem, stem + ".c",
                                      function("common") + function("helper")))
        cls.paths.append(assemble(cls.directory, "globals", "allglobal.c",
                                  function("exported", "globl", 3) +
                                  function("weak_export", "weak", 2)))
        for index in range(2):
            cls.paths.append(assemble(cls.directory, "dup%d" % index, "dup.c",
                                      function("dup_%d" % index, "globl")))
        cls.blob = cls.directory / "linked.o"
        subprocess.run(["ld", "-m", "elf_i386", "-r", "-o", str(cls.blob)] +
                       [str(p) for p in cls.paths], check=True, capture_output=True)
        cls.b = tuattrib.build(str(cls.blob))

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_globals_beyond_sh_info_are_not_last_file_locals(self):
        sections = elfinfo.read_sections(str(self.blob))
        syms = elfinfo.read_symbols(str(self.blob))
        data = self.blob.read_bytes()
        shoff = struct.unpack_from("<I", data, 32)[0]
        entsize = struct.unpack_from("<H", data, 46)[0]
        symtab = next(int(i) for i, s in sections.items() if s.name == ".symtab")
        boundary = struct.unpack_from("<I", data, shoff + symtab * entsize + 28)[0]
        locals_ = [s for s in syms if s.bind == "LOCAL"]
        globals_ = [s for s in syms if s.bind in ("GLOBAL", "WEAK")]
        self.assertEqual(len(globals_), 4)
        self.assertTrue(all(s.num < boundary for s in locals_))
        self.assertTrue(all(s.num >= boundary for s in globals_))
        self.assertTrue(all(s.num > max(t.num for t in syms if t.type == "FILE")
                            for s in globals_))
        self.assertEqual(len(self.b["truth"]), 4)
        self.assertTrue(all(self.b["symbols"][k]["bind"] == "LOCAL"
                            for k in self.b["truth"]))

    def test_all_global_nonempty_tu_is_unanchored_not_empty(self):
        mapped = tumap.build_map(str(self.blob))
        problems, stats = tumap.validate(mapped)
        self.assertEqual(problems, [])
        tu = mapped["tus"]["allglobal.c"]
        self.assertEqual(tu["locals"], [])
        self.assertEqual(tu["kind"], "bracket")
        self.assertGreater(tu["hi"], tu["lo"])
        self.assertEqual(stats["globals"], 4)
        self.assertEqual(stats["ambiguous"], 4)
        self.assertNotIn("exported", self.b["truth"])
        self.assertNotIn("exported", self.b["attrib"])

    def test_duplicate_local_occurrences_do_not_collapse(self):
        b = self.b
        self.assertEqual(len(b["funcs"]), 8)
        self.assertEqual(len(b["truth"]), 4)
        for name in ("common", "helper"):
            keys = [k for k, s in b["symbols"].items() if s["name"] == name]
            self.assertEqual(len(keys), 2)
            self.assertEqual({b["truth"][k] for k in keys}, {"a.c", "b.c"})
            self.assertEqual(len({b["symbols"][k]["num"] for k in keys}), 2)
            self.assertEqual({(b["symbols"][k]["type"], b["symbols"][k]["section"],
                              b["symbols"][k]["size"]) for k in keys},
                             {("FUNC", ".text", 1)})

    def test_duplicate_filename_candidates_remain_ambiguous(self):
        for name in ("dup_0", "dup_1"):
            tu, how = self.b["attrib"][name]
            self.assertEqual(how, "ambig-stem")
            self.assertEqual(set(tu.split("|")), {"dup.c#3", "dup.c#4"})

    def test_json_retains_unknown_symbols_and_denominators(self):
        report = self.directory / "attribution.json"
        result = subprocess.run([sys.executable, str(Path(tuattrib.__file__)),
                                 str(self.blob), "--json", str(report), "--verify"],
                                check=True, capture_output=True, text=True)
        data = json.loads(report.read_text())
        self.assertEqual(data["functions"], 8)
        self.assertEqual(data["local_truth"], 4)
        self.assertEqual(data["verified"], 0)
        self.assertEqual(len(data["symbols"]), 8)
        self.assertNotIn("exported", data["attrib"])
        self.assertIn("exported", data["symbols"])
        self.assertIn("verification coverage: 0/4 local occurrences; 0/8 functions",
                      result.stdout)

    def test_duplicate_names_keep_separate_inferences(self):
        paths = []
        for stem in ("x", "y"):
            paths.append(assemble(self.directory, stem, stem + ".c",
                                  function(stem + "_first") + function("middle") +
                                  function(stem + "_last")))
        blob = self.directory / "fill.o"
        subprocess.run(["ld", "-m", "elf_i386", "-r", "-o", str(blob)] +
                       [str(p) for p in paths], check=True, capture_output=True)
        b = tuattrib.build(str(blob))
        keys = [k for k, s in b["symbols"].items() if s["name"] == "middle"]
        self.assertEqual(len(keys), 2)
        self.assertEqual({b["attrib"][k] for k in keys},
                         {("x.c", "fill"), ("y.c", "fill")})
        self.assertTrue(all(b["attrib"][k][0] == b["truth"][k] for k in keys))

    def order(self, attribution, source="src/renamed.c"):
        return recoverorder.recover([(0, "globals.o", source)], self.directory,
                                    recoverorder.reference(self.blob), attribution)[0]

    def test_order_rejects_legacy_ambiguous_and_duplicate_basename(self):
        for item in ({"tu": "a.c", "how": "ambig-stem"},
                     {"tu": "dup.c", "how": "prefix"},
                     {"tu": "a.c|b.c", "how": "prefix"}):
            row = self.order({"exported": item})
            self.assertIsNone(row["chosen"])
            self.assertEqual(row["classification"], "unknown")

    def test_order_conflict_is_ambiguous_not_earliest(self):
        row = self.order({"exported": {"tu": "a.c", "how": "prefix"},
                          "weak_export": {"tu": "b.c", "how": "class"}})
        self.assertIsNone(row["chosen"])
        self.assertEqual(row["classification"], "ambiguous")
        self.assertEqual(len(row["evidence"]), 2)

    def test_order_unique_occurrence_and_filename_policy(self):
        row = self.order({"exported": {"tu": "dup.c#4", "how": "prefix"}})
        self.assertEqual(row["chosen"][:3], (4, "attributed-symbol", "dup.c#4"))
        row = self.order({"exported": {"tu": "a.c", "how": "prefix"}},
                         source="src/allglobal.c")
        self.assertEqual(row["chosen"][:3], (2, "filename", "allglobal.c"))
        self.assertEqual(row["classification"], "ordering-candidate")

    def test_order_unknown_retains_input_order(self):
        rows = [(0, "globals.o", "src/unknown.c"), (1, "a.o", "src/a.c"),
                (2, "b.o", "src/other.c")]
        result = recoverorder.recover(rows, self.directory,
                                      recoverorder.reference(self.blob), {})
        self.assertEqual([r["input_index"] for r in result], [1, 0, 2])
        self.assertEqual([r["classification"] for r in result],
                         ["ordering-candidate", "unknown", "unknown"])

    def test_tumap_known_bad_extent_detector_fires(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            tumap.self_test()
        self.assertIn("detector fires", output.getvalue())


def run():
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(AttributionTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    print("TU attribution/order self-test: %d/%d passed (ELF fixture: 5 FILE "
          "occurrences, 4 local functions, 4 GLOBAL/WEAK functions)" %
          (result.testsRun - len(result.failures) - len(result.errors), result.testsRun))
    if not result.wasSuccessful():
        raise SystemExit(1)


if __name__ == "__main__":
    run()
