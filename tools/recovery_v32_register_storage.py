#!/usr/bin/env python3
"""Replay and verify the bounded V.32 register-storage source experiment.

Default mode generates four isolated source forms and compiles their 14
transitive v32struct.h consumers with the shared Gentoo period helper. --audit
cheaply verifies the immutable retained build evidence without compiling.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import shlex
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from experiment_toolchain import compile_shell, docker_prefix, print_identity


PIN = "dfc23b7d1e92f5913b22b124771211d3e4650771"
IMAGE = "ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest"
COMPILER = "/usr/i386-pc-linux-gnu/gcc-bin/3.4"
EXPECTED_INPUTS = {
    "include/dsplib/v32seq.h": "205290620e820311ec42312f607cf63239f9f639328ba7fb6e9395242c608340",
    "include/dsplib/v32struct.h": "45581d02c8b9181130fa405d1fad088a00c402d3ccf01ad6cd9338010aba9be5",
    "src/pump/v32/V32.c": "20a30b0947d5d29c0b18e6b5bafbc378d6042ba32f8a5fdf046ababa5bc760f4",
    "src/pump/v32/V32org.c": "035b87d2a5b9e0a70b96b0879b51465dac08f23ff09b2f335d687c208a087197",
    "src/pump/v32/V32rxhdx.c": "b7633bc25fc412357e68e730fc5ca8df33cdc9952cf2b523be651596f6c08f82",
    "src/pump/v32/V32RNG.c": "ac3b2e674b7510f52262533efd1290aeab7bed972818e6cd9ec90d5ab97a5c55",
}
CONFIG_SHA256 = "929f2f43cc2149a5f005aadca62b684c9b74c661e32ba26c6d5c1f6c79d223c7"
MANIFEST_SHA256 = "d9b70bfc74c15169d8b416ac032b7cd6e2124e2b2ba6c9f57b38a605a2ec8bf4"
HELPER_SHA256 = "19bee00f81b79e2ce2380fd8208e4597bffdcc2b96a22acf594cfd3064bfe5c4"
PERIOD_COMPAT_SHA256 = "49e1dbd4585703dc5f59990800133df247d0ceba439f61bfa53596040c08440a"
EXPECTED_FLAGS = (
    "-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 "
    "-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args "
    "-Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h "
    "-DDSPLIB_REPRODUCE_BUGS -DDSPLIB_REPRODUCE_BUGS"
)
AUDIT_ARTIFACTS = {
    "build/tc_repro/.build-config": CONFIG_SHA256,
    "build/tc_repro/tc_manifest.txt": MANIFEST_SHA256,
    "build/typed-offset-audit293/four-cell-results.tsv": "03403b17aeb1e723d81f721ca6a2b331b5b274ae24b7b284271c73ad6a947602",
    "build/typed-offset-audit293/storage-model-replay.log": "753fc1763d972d5bac6a4b053db053a10da449a6808b0645f6029b323325a3ff",
    "build/typed-offset-audit293/bounds-candidate/explicit/period/t_v32fprecr.run.log": "efd0c371a75c8574e0b513bb11b5e7c20544ca0d15dd4506212941bd8c900823",
    "build/typed-offset-audit293/bounds-candidate/explicit/period/t_v32fpdisp.run.log": "2f46305c254620e2f82d654bde96b4fc139309183e3e47db2d4fc4a4110c589d",
}
EXPECTED_CANONICAL_OBJECTS = {
    "src_pump_v32_V32.c.o": "eea9b96ebf76b812c85a0a0cd4788749e6019dd501b857ba02e51b79ab37cac8",
    "src_pump_v32_V32RNG.c.o": "2398922e1c5b2da67003bcda834b46de404a25c7519ea63e08f4ebf739989522",
    "src_pump_v32_V32TXHDX.c.o": "df476a768567407cf60f82dc6a31ef325438bb3ed746082ea1b02d596751d45f",
    "src_pump_v32_V32ans.c.o": "5a5c983af03605197df3a215343d06c0b1aa20ac337c793be2145827b8b2213b",
    "src_pump_v32_V32int.c.o": "c2902393cc85da675182d63a42d46a46a1df1453e6b22bdbe41c1395b9bce2f8",
    "src_pump_v32_V32loop.c.o": "cf2caef0edc9611b8adde4d20995e46675e0a8a79a318e599f5ae4e358133c69",
    "src_pump_v32_V32mod.c.o": "738db41d66b409f09256b7f089cfcf4f01a702b455001ce05914103e5a4fea48",
    "src_pump_v32_V32org.c.o": "fed1c01227882106bcc6b2aeb93e3f5a02e87575393c5e4ab2ce6c53534c05c4",
    "src_pump_v32_V32prc.c.o": "8b3326738d5c7dbbc1e9226a1a5c8a84699e7c3e92150c3cd5b4f8aeb5e5e319",
    "src_pump_v32_V32rxhdx.c.o": "af4ee86e0d67e6c06bcd5e0d11d1f15595e276948b7a754728f4231c4c99055d",
    "src_pump_v32_V32stc.c.o": "348f1495c6da961029914a9904b6e645884785bc766cff9a9ce7e8687085d542",
    "src_pump_v32_v32.c.o": "dfe41fb860d72d960e476ff380cedc26522ed4aefc59cb90c405ae87a0fd4cd8",
    "src_pump_v32_v32fptab.c.o": "2842ce94be7fa89a889ec226c82adfcbe852bf0de7d269b699a038917c4f487f",
    "src_pump_v32_v32hdx_tables.c.o": "f13a9c71015766a156e9c66d1c69b400c6bfec464f6475d26a4dc1842dd1b904",
}


def find_root() -> Path:
    for candidate in Path(__file__).resolve().parents:
        if (candidate / "Makefile").is_file() and (candidate / ".git").exists():
            return candidate
    raise SystemExit("could not find repository root from this script path")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def pinned_bytes(root: Path, rel: str) -> bytes:
    return subprocess.check_output(["git", "show", f"{PIN}:{rel}"], cwd=root, stderr=subprocess.DEVNULL)


def require_pinned_inputs(root: Path) -> None:
    for rel, expected in EXPECTED_INPUTS.items():
        historical = pinned_bytes(root, rel)
        live = (root / rel).read_bytes()
        if digest(historical) != expected or live != historical:
            # V32.c is expected to evolve when the separately reviewed
            # constructor correction is applied. Replay always compiles the
            # immutable pinned bytes below, never that live file.
            if rel != "src/pump/v32/V32.c" or digest(historical) != expected:
                raise SystemExit(f"pinned input mismatch: {rel}")


def require_hash(root: Path, rel: str, expected: str) -> bytes:
    path = root / rel
    if not path.is_file():
        raise SystemExit(f"missing retained audit artifact: {rel}")
    data = path.read_bytes()
    actual = digest(data)
    if actual != expected:
        raise SystemExit(f"artifact hash mismatch: {rel}: {actual} != {expected}")
    return data


def check_fixture_log(data: bytes, name: str, groups_expected: int, checks_expected: int) -> None:
    groups = checks = 0
    for line in data.decode("utf-8").splitlines():
        fields = line.rsplit(" ", 2)
        if len(fields) == 3 and fields[0].startswith("PASS ") and fields[2] == "checks":
            try:
                checks += int(fields[1])
            except ValueError:
                continue
            groups += 1
    if (groups, checks) != (groups_expected, checks_expected):
        raise SystemExit(f"{name}: fixture denominator {groups}/{checks}, expected {groups_expected}/{checks_expected}")
    print(f"{name}: PASS {groups} groups / {checks} checks (existing period run)")


def audit_mode(root: Path) -> None:
    require_pinned_inputs(root)
    config = require_hash(root, "build/tc_repro/.build-config", CONFIG_SHA256)
    manifest_bytes = require_hash(root, "build/tc_repro/tc_manifest.txt", MANIFEST_SHA256)
    require_hash(root, "tools/experiment_toolchain.py", HELPER_SHA256)
    require_hash(root, "tools/toolchain/period_compat.h", PERIOD_COMPAT_SHA256)
    flags = next((line[6:] for line in config.decode().splitlines() if line.startswith("flags ")), None)
    if flags != EXPECTED_FLAGS:
        raise SystemExit("period config flags differ from recorded complete command flags")
    table = require_hash(root, "build/typed-offset-audit293/four-cell-results.tsv", AUDIT_ARTIFACTS["build/typed-offset-audit293/four-cell-results.tsv"])
    rows = list(csv.DictReader(table.decode().splitlines(), delimiter="\t"))
    if len(rows) != 14:
        raise SystemExit(f"four-cell denominator changed: {len(rows)} != 14")
    counts = {key: sum(row[key] == "True" for row in rows) for key in ("B_eq_A", "C_eq_A", "D_eq_C")}
    if counts != {"B_eq_A": 14, "C_eq_A": 13, "D_eq_C": 14}:
        raise SystemExit(f"four-cell object comparison mismatch: {counts}")
    require_hash(root, "build/typed-offset-audit293/storage-model-replay.log", AUDIT_ARTIFACTS["build/typed-offset-audit293/storage-model-replay.log"])
    check_fixture_log(require_hash(root, "build/typed-offset-audit293/bounds-candidate/explicit/period/t_v32fprecr.run.log", AUDIT_ARTIFACTS["build/typed-offset-audit293/bounds-candidate/explicit/period/t_v32fprecr.run.log"]), "t_v32fprecr", 6, 583756)
    check_fixture_log(require_hash(root, "build/typed-offset-audit293/bounds-candidate/explicit/period/t_v32fpdisp.run.log", AUDIT_ARTIFACTS["build/typed-offset-audit293/bounds-candidate/explicit/period/t_v32fpdisp.run.log"]), "t_v32fpdisp", 7, 106013)
    print(f"audit PASS: source pin {PIN}; manifest {digest(manifest_bytes)}; 14 consumers; B=A 14/14, C=A 13/14, D=C 14/14")


def replace_once(text: str, old: str, new: str, count: int, label: str) -> str:
    actual = text.count(old)
    if actual != count:
        raise SystemExit(f"source transform anchor {label}: expected {count} matches, found {actual}")
    return text.replace(old, new)


def write_overlay(root: Path, out: Path, seven_array: bool, explicit: bool, pinned_dependencies: set[str]) -> Path:
    overlay = out / "overlay"
    for rel in sorted(pinned_dependencies):
        path = overlay / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(pinned_bytes(root, rel))
    for rel in ("include/dsplib/v32struct.h", "src/pump/v32/V32.c", "src/pump/v32/V32org.c", "src/pump/v32/V32rxhdx.c", "src/pump/v32/V32RNG.c"):
        path = overlay / rel
        if not path.is_file():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(pinned_bytes(root, rel))
        source = path.read_text()
        if seven_array and rel == "include/dsplib/v32struct.h":
            source = replace_once(source, "\tshort regs[V32_SEQ_REGS];\n\tshort short_46, short_48;", "\tshort regs[7];", 1, "seven-element storage declaration")
        if seven_array and rel == "src/pump/v32/V32org.c":
            source = replace_once(source, "short_46", "regs[5]", 2, "V32org last two slots")
        if seven_array and rel == "src/pump/v32/V32rxhdx.c":
            source = replace_once(source, "short_48", "regs[6]", 6, "V32rxhdx seventh slot")
        if seven_array and rel == "src/pump/v32/V32RNG.c":
            source = replace_once(source, "short_48", "regs[6]", 2, "V32RNG seventh slot")
        if explicit and rel == "src/pump/v32/V32.c":
            source = replace_once(source, "#define V32HDX_REGS_CLEARED\t7\n", "", 1, "remove loop-bound macro")
            loop = "\tfor (i = 0; i < V32HDX_REGS_CLEARED; i++)\n\t\thdx->regs[i] = 0;"
            final_stores = ("\thdx->regs[5] = 0;\n\thdx->regs[6] = 0;" if seven_array
                            else "\thdx->short_46 = 0;\n\thdx->short_48 = 0;")
            stores = "\t" + "\n\t".join(f"hdx->regs[{i}] = 0;" for i in range(5)) + "\n" + final_stores
            source = replace_once(source, loop, stores, 1, "five-plus-two explicit constructor stores")
            if source.count("V32HDX_REGS_CLEARED"):
                raise SystemExit("obsolete V32HDX_REGS_CLEARED use remains")
        path.write_text(source)
    return overlay


def consumers(root: Path) -> tuple[list[tuple[str, str]], set[str]]:
    manifest = {}
    for line in (root / "build/tc_repro/tc_manifest.txt").read_text().splitlines():
        if line.strip():
            obj, source = line.split(None, 1)
            manifest[obj] = source
    found = []
    depfiles = {}
    for dep in (root / "build/tc_repro/.deps").glob("*.d"):
        if "include/dsplib/v32struct.h" in dep.read_text(errors="ignore"):
            obj = dep.name[:-2]
            if obj in manifest:
                found.append((obj, manifest[obj]))
                depfiles[obj] = dep
    found.sort()
    if len(found) != 14:
        raise SystemExit(f"transitive consumer denominator changed: {len(found)} != 14")
    for obj, source in found:
        if digest(pinned_bytes(root, source)) == "":
            raise SystemExit(f"cannot read pinned consumer source: {source}")
    pinned_dependencies = {source for _obj, source in found}
    for obj, _source in found:
        dep_text = depfiles[obj].read_text(errors="ignore").replace("\\\n", " ")
        if ":" not in dep_text:
            raise SystemExit(f"malformed dependency file: {depfiles[obj]}")
        for token in shlex.split(dep_text.split(":", 1)[1]):
            if token.endswith(":"):
                # GCC -MMD emits a phony rule for each dependency after the
                # primary dependency line. Those tokens are targets, not paths.
                continue
            path = Path(token)
            if path.is_absolute():
                try:
                    path = path.relative_to(root)
                except ValueError:
                    continue
            if not path.parts or path.parts[0] == "build":
                continue
            rel = path.as_posix()
            try:
                pinned_bytes(root, rel)
            except subprocess.CalledProcessError:
                continue
            pinned_dependencies.add(rel)
    return found, pinned_dependencies


def replay_mode(root: Path) -> None:
    require_pinned_inputs(root)
    if digest((root / "tools/experiment_toolchain.py").read_bytes()) != HELPER_SHA256:
        raise SystemExit("shared experiment helper differs from immutable audit pin")
    if digest((root / "tools/toolchain/period_compat.h").read_bytes()) != PERIOD_COMPAT_SHA256:
        raise SystemExit("period compatibility header differs from immutable audit pin")
    config_data = (root / "build/tc_repro/.build-config").read_bytes()
    if digest(config_data) != CONFIG_SHA256:
        raise SystemExit("canonical period .build-config hash differs from audit pin")
    config_lines = config_data.decode().splitlines()
    flags = next((line[6:] for line in config_lines if line.startswith("flags ")), None)
    if flags != EXPECTED_FLAGS:
        raise SystemExit("canonical period flags differ from recorded full flags")
    if digest((root / "build/tc_repro/tc_manifest.txt").read_bytes()) != MANIFEST_SHA256:
        raise SystemExit("canonical period manifest hash differs from audit pin")
    units, pinned_dependencies = consumers(root)
    output = root / "build/v32-register-storage-replay"
    output.mkdir(parents=True, exist_ok=True)
    manifest_path = output / "transitive-consumers.tsv"
    manifest_path.write_text("object\tsource\n" + "\n".join(f"{obj}\t{src}" for obj, src in units) + "\n")
    base_flags = flags.split()
    # The shared helper appends DSPLIB_REPRODUCE_BUGS after these flags.
    if base_flags[-1] != "-DDSPLIB_REPRODUCE_BUGS":
        raise SystemExit("period flags lack final DSPLIB_REPRODUCE_BUGS define")
    base_flags = base_flags[:-1]
    cells = {
        "A_5plus2_loop": (False, False),
        "B_7array_loop": (True, False),
        "C_5plus2_explicit": (False, True),
        "D_7array_explicit": (True, True),
    }
    overlays = {name: write_overlay(root, output / name, seven, explicit, pinned_dependencies) for name, (seven, explicit) in cells.items()}
    log_lines = [f"pin={PIN}", f"image={IMAGE}", f"consumer_denominator={len(units)}", f"flags_from_build_config={flags}"]
    print_identity(IMAGE, COMPILER, True)
    log_lines.append(f"compiler_identity=GCC 3.4.2 Gentoo 3.4.2-r2; GNU as 2.15.92.0.2; image {IMAGE}")
    objects: dict[str, dict[str, bytes]] = {name: {} for name in cells}

    def cell_flags(cell: str) -> list[str]:
        # All headers, including the forced compatibility header, must come
        # from the pinned overlay. `-Iinclude` remains later as a fallback.
        result = []
        i = 0
        while i < len(base_flags):
            if base_flags[i] == "-include":
                i += 2
                continue
            result.append(base_flags[i])
            i += 1
        overlay_root = Path("build/v32-register-storage-replay") / cell / "overlay"
        return [f"-I{overlay_root / 'include'}", *result, "-include", str(overlay_root / "tools/toolchain/period_compat.h")]

    def compile_command(cell: str, flags_for_cell: list[str], source: Path, dest: Path) -> list[str]:
        return docker_prefix(IMAGE, str(root), str(root), True) + [
            "/bin/sh", "-c", "set -e; " + compile_shell(
                COMPILER, flags_for_cell, "/src/" + str(dest.relative_to(root)), source
            )
        ]

    for cell, (seven, _explicit) in cells.items():
        useflags = cell_flags(cell)
        for obj, source in units:
            candidate_source = str((overlays[cell] / source).relative_to(root))
            dest = output / cell / "objects" / obj
            dest.parent.mkdir(parents=True, exist_ok=True)
            cmd = compile_command(cell, useflags, Path(candidate_source), dest)
            log_lines.append(f"{cell}\t{source}\t{shlex.join(cmd)}")
            subprocess.run(cmd, cwd=root, check=True)
            objects[cell][obj] = dest.read_bytes()
            print(f"compiled {cell}: {source}", flush=True)

    for obj, _source in units:
        if digest(objects["A_5plus2_loop"][obj]) != EXPECTED_CANONICAL_OBJECTS[obj]:
            raise SystemExit(f"pinned A baseline object hash mismatch: {obj}")
    counts = {
        "B_eq_A": sum(objects["B_7array_loop"][obj] == objects["A_5plus2_loop"][obj] for obj, _ in units),
        "C_eq_A": sum(objects["C_5plus2_explicit"][obj] == objects["A_5plus2_loop"][obj] for obj, _ in units),
        "D_eq_C": sum(objects["D_7array_explicit"][obj] == objects["C_5plus2_explicit"][obj] for obj, _ in units),
    }
    if counts != {"B_eq_A": 14, "C_eq_A": 13, "D_eq_C": 14}:
        raise SystemExit(f"four-cell comparison changed: {counts}")
    result_lines = ["source\tobject\tA_sha256\tB_sha256\tC_sha256\tD_sha256\tB_eq_A\tC_eq_A\tD_eq_C"]
    for obj, source in units:
        a, b, c, d = (objects[key][obj] for key in cells)
        result_lines.append("\t".join((source, obj, digest(a), digest(b), digest(c), digest(d), str(a == b), str(a == c), str(c == d))))
    (output / "four-cell-results.tsv").write_text("\n".join(result_lines) + "\n")
    log_lines.extend((f"baseline_controls={len(units)}/{len(units)}", f"four_cell_counts={counts}"))
    (output / "commands.log").write_text("\n".join(log_lines) + "\n")

    # Compile-time layout assertions prove which declaration each cell used.
    # A deliberate negative control with the wrong include path must fail.
    for cell, (seven, _explicit) in cells.items():
        expected_bytes = 14 if seven else 10
        probe_dir = output / cell / "probes"
        probe_dir.mkdir(parents=True, exist_ok=True)
        probe = probe_dir / "layout_probe.c"
        probe.write_text(
            "#include <dsplib/v32struct.h>\n"
            f"typedef char v32_regs_size_must_be_{expected_bytes}["
            f"sizeof(((struct v32_hdx *)0)->regs) == {expected_bytes} ? 1 : -1];\n"
            "int v32_layout_probe(void) { return sizeof(((struct v32_hdx *)0)->regs); }\n"
            + ("int v32_fields_present(struct v32_hdx *p) { return p->short_46 + p->short_48; }\n" if not seven else "")
        )
        probe_obj = probe_dir / "layout_probe.o"
        cmd = compile_command(cell, cell_flags(cell), Path(str(probe.relative_to(root))), probe_obj)
        log_lines.append(f"LAYOUT {cell}\t{shlex.join(cmd)}")
        (output / "commands.log").write_text("\n".join(log_lines) + "\n")
        subprocess.run(cmd, cwd=root, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        log_lines.append(f"LAYOUT_ASSERTION {cell}\tsizeof(v32_hdx.regs)=={expected_bytes} bytes; compile passed")
        expected_header = f"build/v32-register-storage-replay/{cell}/overlay/include/dsplib/v32struct.h"
        trace_cmd = docker_prefix(IMAGE, str(root), str(root), True) + [
            "/bin/sh", "-c", "set -e; export PATH=/usr/i386-pc-linux-gnu/gcc-bin/3.4:$PATH; exec "
            + shlex.join(["gcc", "-E", "-H", *cell_flags(cell), "-DDSPLIB_REPRODUCE_BUGS", "/src/" + str(probe.relative_to(root)), "-o", "/dev/null"])
        ]
        trace = subprocess.run(trace_cmd, cwd=root, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if expected_header not in trace.stderr:
            raise SystemExit(f"{cell}: preprocessor did not select pinned overlay header; expected {expected_header}")
        trace_line = next(line.strip() for line in trace.stderr.splitlines() if expected_header in line)
        log_lines.append(f"HEADER_TRACE {cell}\t{trace_line}")
        if seven:
            absent = probe_dir / "absent_fields_control.c"
            absent.write_text("#include <dsplib/v32struct.h>\nint absent_fields(struct v32_hdx *p) { return p->short_46 + p->short_48; }\n")
            absent_obj = probe_dir / "absent_fields_control.o"
            absent_cmd = compile_command(cell, cell_flags(cell), Path(str(absent.relative_to(root))), absent_obj)
            rejected = subprocess.run(absent_cmd, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if rejected.returncode == 0:
                raise SystemExit(f"{cell}: removed short_46/short_48 fields unexpectedly compiled")
            if b"short_46" not in rejected.stderr and b"short_48" not in rejected.stderr:
                raise SystemExit(f"{cell}: absent-fields control failed for an unrelated compile error")
            log_lines.append(f"ABSENT_FIELDS_CONTROL {cell}\trejected as expected")

    wrong_dir = output / "wrong-include-control"
    wrong_header = wrong_dir / "include/dsplib/v32struct.h"
    wrong_header.parent.mkdir(parents=True, exist_ok=True)
    wrong_header.write_bytes(pinned_bytes(root, "include/dsplib/v32struct.h"))
    wrong_probe = wrong_dir / "must_reject_wrong_header.c"
    wrong_probe.write_text(
        "#include <dsplib/v32struct.h>\n"
        "typedef char seven_register_words_required[sizeof(((struct v32_hdx *)0)->regs) == 14 ? 1 : -1];\n"
    )
    wrong_flags = [f"-I{wrong_dir.relative_to(root) / 'include'}", *cell_flags("D_7array_explicit")]
    wrong_obj = wrong_dir / "wrong.o"
    wrong_cmd = compile_command("D_7array_explicit", wrong_flags, Path(str(wrong_probe.relative_to(root))), wrong_obj)
    log_lines.append(f"WRONG_INCLUDE_CONTROL_COMMAND\t{shlex.join(wrong_cmd)}")
    (output / "commands.log").write_text("\n".join(log_lines) + "\n")
    wrong_result = subprocess.run(wrong_cmd, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if wrong_result.returncode == 0:
        raise SystemExit("wrong-include control unexpectedly compiled; include precedence control is broken")
    if b"seven_register_words_required" not in wrong_result.stderr or b"negative" not in wrong_result.stderr:
        raise SystemExit("wrong-include control failed for a reason other than the expected 14-byte layout assertion")
    log_lines.append("WRONG_INCLUDE_CONTROL\trejected seven-array layout when five-plus-two pinned header was first")
    (output / "commands.log").write_text("\n".join(log_lines) + "\n")
    print(f"four-cell replay PASS in {output.relative_to(root)}: {counts}; independent A baseline controls {len(units)}/{len(units)}")
    print("Original physical storage extent remains unresolved; replay compiles isolated overlays only.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audit", action="store_true", help="verify retained artifact hashes and denominators without compiling")
    args = parser.parse_args()
    root = find_root()
    if args.audit:
        audit_mode(root)
    else:
        replay_mode(root)


if __name__ == "__main__":
    main()
