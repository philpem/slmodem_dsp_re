#!/usr/bin/env python3
"""Run and validate the V32 typed-owner audit probe with period GCC."""
from pathlib import Path
import argparse
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "build/tc_repro/.build-config"
WORK = ROOT / "build/typed-offset-audit293"
SOURCE = "tools/recovery_typed_owner_probe.c"
OUTPUT = "/work/recovery_typed_owner_probe.o"
EXECUTABLE = "/work/recovery_typed_owner_probe"

sys.path.insert(0, str(ROOT / "tools"))
import experiment_toolchain as toolchain


def read_config(path):
    values = {}
    for line in path.read_text().splitlines():
        key, _, value = line.partition(" ")
        values[key] = value.strip()
    if not values.get("image") or not values.get("flags"):
        raise SystemExit("period .build-config must contain image and flags")
    return values


def parse_rows(text):
    rows = {}
    for line in text.splitlines():
        match = re.fullmatch(r"([\w.\[\]]+) off=(\d+) size=(\d+) signed=(n/a|[01])", line)
        if match:
            name, offset, size, signed = match.groups()
            if name in rows:
                raise SystemExit("duplicate probe member: " + name)
            rows[name] = (int(offset), int(size), signed)
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=CONFIG)
    args = parser.parse_args()
    if not args.config.is_file():
        raise SystemExit("period configuration not found: %s" % args.config)
    WORK.mkdir(parents=True, exist_ok=True)
    config = read_config(args.config)
    image = config["image"]
    flags = [flag for flag in shlex.split(config["flags"])
             if flag != toolchain.REPRODUCE_BUGS]
    gcc_path = toolchain.GENTOO_COMPILER_PATH
    compile_command = toolchain.compile_shell(gcc_path, flags, OUTPUT, SOURCE)
    if toolchain.REPRODUCE_BUGS not in compile_command:
        raise SystemExit("shared helper omitted DSPLIB_REPRODUCE_BUGS")
    compile_command = compile_command.replace("exec gcc -c", "gcc -c", 1)
    docker = toolchain.docker_prefix(
        image, str(ROOT), str(WORK), True, work_target="/work")
    shell = (compile_command + " && gcc " + shlex.quote(OUTPUT) +
             " -o " + shlex.quote(EXECUTABLE) + " && " +
             shlex.quote(EXECUTABLE))
    command = docker + ["/bin/sh", "-c", shell]

    toolchain.print_identity(image, gcc_path, True)
    print("period .build-config:", args.config)
    print("period flags:", shlex.join(flags + [toolchain.REPRODUCE_BUGS]))
    print("probe command:", shlex.join(command), flush=True)
    result = subprocess.run(command, cwd=ROOT, check=True,
                            capture_output=True, text=True)
    print(result.stdout, end="")
    rows = parse_rows(result.stdout)

    # The complete v32seq/v32fpctl ledgers and changed mappings. The separate
    # RXHDX-40 and V32.c-40 sample probes remain outside this denominator.
    ordinary = {
        "v32_hdx.gen_index": (0x4a, 2, "0"),
        "v32_hdx.gen_index_mask": (0x4c, 2, "0"),
        "v32_hdx.gen_width": (0x4e, 2, "0"),
        "v32_hdx.gen_mask": (0x50, 2, "0"),
        "v32_hdx.gen_pattern": (0x52, 2, "0"),
        "v32_hdx.det_width": (0x54, 2, "0"),
        "v32_hdx.det_out_mask": (0x58, 4, "1"),
        "v32_hdx.det_target": (0x5c, 4, "1"),
        "v32_hdx.det_mask": (0x60, 4, "1"),
        "v32_hdx.det_reg": (0x64, 4, "1"),
        "v32_hdx.det_match": (0x68, 4, "1"),
        "v32_hdx.mtd": (0x38, 4, "n/a"),
        "v32_hdx.tone2": (0x34, 4, "n/a"),
        "v32_hdx.tone1": (0x30, 4, "n/a"),
        "v32_hdx.tone0": (0x2c, 4, "n/a"),
        "v32_fp.decoder.vtb.paths": (0x5038, 4, "n/a"),
        "v32_fp.sre": (0x148, 144, "n/a"),
        "v32_fp.mrf": (0xc4, 28, "n/a"),
        "v32_fp.rx_buf": (0x50cc, 4, "n/a"),
        "v32_fp.clean_buf": (0x50d0, 4, "n/a"),
        "v32_hdx.buffer": (0xa4, 4, "n/a"),
        "v32_fp.clean_n": (0x50d4, 2, "0"),
        "v32_fp.rx_smc.shift": (0x509c, 2, "1"),
        "v32_fp.rx_smc.mode": (0x5098, 2, "1"),
        "v32_dec.short_04": (0x04, 2, "1"),
        "v32_fp.short_2e": (0x2e, 2, "1"),
        "v32_modem.status": (0x30, 1, "0"),
        "v32_modem.flags": (0x31, 1, "0"),
        "v32_sdm.tap1_pos": (0x02, 2, "1"),
        "v32_sdm.tap2_pos": (0x04, 2, "1"),
    }
    # Additional distinct paths from the complete v32seq/v32fpctl worksheets,
    # including the nine changed owner-mapping macros and address-only VTB
    # arguments. Expectations are static values from the period probe.
    ordinary.update({
        "v32_modem.params": (0x00, 48, "n/a"),
        "v32_modem.hdx": (0x64, 4, "n/a"),
        "v32_modem.fp": (0x68, 4, "n/a"),
        "v32_modem.params.rx_rate": (0x04, 2, "1"),
        "v32_modem.params.trellis": (0x1c, 4, "1"),
        "v32_modem.params.ec_near_delay": (0x14, 2, "0"),
        "v32_modem.params.symlen_sel": (0x18, 2, "1"),
        "v32_fp.scrambler": (0x30, 24, "n/a"),
        "v32_fp.descrambler": (0x50b0, 24, "n/a"),
        "v32_fp.tx_smc": (0x48, 22, "n/a"),
        "v32_fp.pps": (0x60, 56, "n/a"),
        "v32_fp.symout": (0xb0, 20, "n/a"),
        "v32_fp.ecc": (0xe0, 104, "n/a"),
        "v32_fp.fse": (0x204, 19992, "n/a"),
        "v32_fp.tx_rate_index": (0x28, 2, "1"),
        "v32_fp.rx_rate_index": (0x2a, 2, "1"),
        "v32_fp.encoder_sel": (0xa4, 2, "1"),
        "v32_fp.short_2c": (0x2c, 2, "1"),
        "v32_fp.int_00": (0x00, 4, "1"),
        "v32_fp.int_04": (0x04, 4, "1"),
        "v32_fp.int_08": (0x08, 4, "1"),
        "v32_fp.int_0c": (0x0c, 4, "1"),
        "v32_fp.eq_adapt": (0x10, 4, "1"),
        "v32_fp.int_14": (0x14, 4, "1"),
        "v32_fp.int_18": (0x18, 4, "1"),
        "v32_fp.decoder": (0x5020, 120, "n/a"),
        "v32_fp.decoder.vtb": (0x5038, 56, "n/a"),
        "v32_fp.decoder.short_04": (0x5024, 2, "1"),
        "v32_fp.decoder.short_74": (0x5094, 2, "0"),
        "v32_hdx.regs": (0x3c, 10, "n/a"),
        "v32_hdx.regs[0]": (0x3c, 2, "1"),
        "v32_hdx.short_9c": (0x9c, 2, "1"),
        "v32_hdx.symbol_len": (0x9e, 2, "1"),
        "v32_hdx.turnaround": (0x94, 2, "1"),
        "v32_hdx.short_98": (0x98, 2, "1"),
        "v32_hdx.short_9a": (0x9a, 2, "1"),
        "v32_dec.vtb": (0x18, 56, "n/a"),
        "v32_dec.short_74": (0x74, 2, "0"),
    })
    positives = {
        "v32_hdx.short_46": (0x46, 2, "1", 0x44),
        "v32_fp.ecc.pwr_out": (0x102, 2, "1", 0x182),
        "v32_fp.ecc.adapt_near": (0x108, 4, "1", 0x188),
        "v32_fp.mrf.need": (0xd4, 2, "1", 0x100),
        "v32_fp.mrf.phase": (0xd6, 2, "1", 0x102),
    }
    if len(ordinary) != 68:
        raise SystemExit("worksheet must cover 68 unique ordinary member paths")
    if len(positives) != 5:
        raise SystemExit("worksheet must retain five selected positive controls")
    if set(rows) != set(ordinary) | set(positives):
        raise SystemExit("probe output member denominator differs from worksheet")
    for name, expected in ordinary.items():
        if rows[name] != expected:
            raise SystemExit("ordinary address/shape mismatch: %s: %r != %r" %
                             (name, rows[name], expected))
    for name, (offset, size, signed, old) in positives.items():
        if rows[name] != (offset, size, signed) or offset == old:
            raise SystemExit("positive control failed: %s" % name)
    print("typed-owner probe: 68 unique ordinary member paths match; "
          "5 selected positive controls mismatch as expected")


if __name__ == "__main__":
    main()
