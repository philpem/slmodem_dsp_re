#!/usr/bin/env python3
"""Wave-2 dump: FDSP_DP_Delete + dp_runtime_create, both sides, raw-byte diff.

Read-only diagnostic. Writes nothing into src/ or include/.
"""
import subprocess, sys, os, glob, json

sys.path.insert(0, 'tools/toolchain')
import byteident as bi  # noqa: E402

SYMS = ("FDSP_DP_Delete", "dp_runtime_create")
OUT = 'build/wave2-dump'
os.makedirs(OUT, exist_ok=True)


def objmap():
    om = {}
    for o in glob.glob(os.path.join(bi.OURS, "*.o")):
        for s in bi.sizes(o):
            om.setdefault(s, o)
    return om


def dis_txt(path, sym):
    return subprocess.run(["objdump", "-d", "--no-show-raw-insn",
                           "--disassemble=" + sym, path],
                          capture_output=True, text=True).stdout


def dis_raw(path, sym):
    return subprocess.run(["objdump", "-dr", "--disassemble=" + sym, path],
                          capture_output=True, text=True).stdout


def main():
    om = objmap()
    summary = {}
    for sym in SYMS:
        ours = om[sym]
        ob, orb = bi.body(ours, sym)
        bb, brb = bi.body(bi.BLOB, sym)
        # raw byte diff
        n = max(len(ob), len(bb))
        diffs = []
        for i in range(n):
            a = ob[i] if i < len(ob) else None
            b = bb[i] if i < len(bb) else None
            if a != b:
                diffs.append((i, a, b))
        # coalesce into runs
        runs = []
        for i, a, b in diffs:
            if runs and i == runs[-1][1] + 1:
                runs[-1][1] = i
                runs[-1][2].append((i, a, b))
            else:
                runs.append([i, i, [(i, a, b)]])
        summary[sym] = {
            'ours_obj': ours,
            'ours_len': len(ob), 'blob_len': len(bb),
            'diff_count': len(diffs),
            'runs': [(r[0], r[1], [(i, a, b) for i, a, b in r[2]]) for r in runs],
            'verdict': bi.verdict(ob, orb, bb, brb),
        }
        with open(f'{OUT}/{sym}.ours.dis', 'w') as f:
            f.write(dis_raw(ours, sym))
        with open(f'{OUT}/{sym}.blob.dis', 'w') as f:
            f.write(dis_raw(bi.BLOB, sym))
        with open(f'{OUT}/{sym}.ours.txt', 'w') as f:
            f.write(dis_txt(ours, sym))
        with open(f'{OUT}/{sym}.blob.txt', 'w') as f:
            f.write(dis_txt(bi.BLOB, sym))
    with open(f'{OUT}/summary.json', 'w') as f:
        json.dump(summary, f, indent=2, default=str)
    for sym in SYMS:
        s = summary[sym]
        print(f"== {sym}: ours {s['ours_len']}B vs blob {s['blob_len']}B, "
              f"{s['diff_count']} differing bytes, verdict {s['verdict'][0]}")
        for lo, hi, rows in s['runs']:
            print(f"  run {lo:#x}..{hi:#x}: {len(rows)} bytes")
        w = s['why']
        if w:
            print("  why:", str(w)[:400])


if __name__ == '__main__':
    main()
