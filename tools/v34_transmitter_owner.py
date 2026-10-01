#!/usr/bin/env python3
"""Four full-TU transmitter-owner controls; compiler comparison only."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b

MEMBERS = {'txq': 'tx.queue', 'txq_ring_tail': 'tx.ring_tail',
           'seg_symcount': 'tx.seg_symcount', 'tx_flags': 'tx.flags'}

def migrate(text):
    for old, new in MEMBERS.items():
        text = re.sub(r'(->|\.)' + old + r'\b', lambda m: m[1] + new, text)
    # Explicit layout assertions name a member without an access operator.
    text = re.sub(r'(V34HS_OFF\(tx_flags,\s*struct v34_object,\s*)tx_flags\b',
                  r'\1tx.flags', text)
    return text

def owner_header(header):
    start = header.index('\tstruct v34_queue txq;')
    end = header.index('\n', header.index('\tshort tx_flags;', start))
    prefix = header[start:end]
    prefix = prefix.replace('struct v34_queue txq;', 'struct v34_queue queue;')
    prefix = prefix.replace('txq_ring_tail[', 'ring_tail[')
    prefix = prefix.replace('short tx_flags;', 'short flags;')
    prefix = prefix.replace('/* +0x221c */', '/* +0x000 */')
    prefix = prefix.replace('/* to +0x25c0 */', '/* +0x010 to +0x3a4 */')
    prefix = prefix.replace('/* +0x25c0 */', '/* +0x3a4 */')
    prefix = prefix.replace('/* +0x25c2 */', '/* +0x3a6 */')
    header = header[:start] + '\tstruct v34_transmitter_prefix tx;\t/* +0x221c to +0x25c4 */' + header[end:]
    declaration = '''/* Independently observed transmitter prefix.  This is not the complete
 * transmitter type or a claim about its original name.  The queue/ring extent
 * and trailing halfwords are bounded by the blob's accesses. */
struct v34_transmitter_prefix {
''' + prefix + '''
};

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char v34tx_prefix_count_offset[
    (__builtin_offsetof(struct v34_transmitter_prefix, seg_symcount) == 0x3a4) ? 1 : -1];
typedef char v34tx_prefix_flags_offset[
    (__builtin_offsetof(struct v34_transmitter_prefix, flags) == 0x3a6) ? 1 : -1];
typedef char v34tx_prefix_extent[
    (sizeof(struct v34_transmitter_prefix) == 0x3a8) ? 1 : -1];
#endif

'''
    return header.replace('struct v34_object {', declaration + 'struct v34_object {', 1)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--revision', default='1f340221')
    ap.add_argument('--order-study', action='store_true', help='Four owner emission-order/peephole controls')
    args = ap.parse_args()
    out = ROOT / ('build/v34-transmitter-order' if args.order_study else 'build/v34-transmitter-owner')
    out.mkdir(parents=True, exist_ok=True)
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    prior = ROOT / ('build/v34-transmitter-owner/owner-before-debug/V34hshak.o' if args.order_study else 'build/tc_out/src_pump_v34_V34hshak.c.o')
    prior_bytes = prior.read_bytes()
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')
    results = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-' + ('5933782577' if args.order_study else '5933706170'), 'revision': args.revision, 'config': config, 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    def saved(path):
        return subprocess.check_output(['git', 'show', args.revision + ':' + path], cwd=ROOT, text=True)
    variants = ('baseline', 'freeze-dma', 'blob-bottom-order', 'no-peephole2') if args.order_study else ('baseline', 'owner-root', 'owner-before-debug', 'owner-after-debug')
    for variant in variants:
        cell = out / variant
        (cell / 'include/dsplib').mkdir(parents=True, exist_ok=True)
        source = saved('src/pump/v34/V34hshak.c')
        header = saved('include/dsplib/v34fsk.h')
        arms = saved('include/dsplib/v34hstx1_arms.h')
        if variant != 'baseline' or args.order_study:
            header = owner_header(header)
            source, arms = migrate(source), migrate(arms)
            if variant != 'owner-root':
                start = source.index('v34FreezeEcho(void *objp)')
                end = source.index('\n}\n', start)
                fn = source[start:end]
                decl = '\n\tstruct v34_transmitter_prefix *tx = &obj->tx;\n'
                anchor = '\tstruct v34_object *obj = (struct v34_object *)objp;\n' if variant != 'owner-after-debug' else '\t\tdsplibs_debug_printf("V34HSHAK: Freeze EC\\n");\n'
                assert fn.count(anchor) == 1
                fn = fn.replace(anchor, anchor + decl, 1).replace('obj->tx.flags', 'tx->flags')
                source = source[:start] + fn + source[end:]
            source += '\n#if __SIZEOF_POINTER__ == 4\ntypedef char v34tx_root_flags[(__builtin_offsetof(struct v34_object, tx.flags) == 0x25c2) ? 1 : -1];\n#endif\n'
        if args.order_study and variant in ('freeze-dma', 'blob-bottom-order'):
            names = ('txrxdmainit', 'v34FreezeEcho', 'V34scrambler', 'V34SetupDemodulator')
            blocks = {}
            starts, ends = [], []
            for fn in names:
                fn_start = source.index('\n' + fn + '(')
                begin = source.rfind('\n/*', 0, fn_start)
                end = source.index('\n}\n', fn_start) + 3
                starts.append(begin); ends.append(end)
                blocks[fn] = source[begin:end]
            begin, end = min(starts), max(ends)
            between = source[begin:end]
            for block in blocks.values():
                between = between.replace(block, '', 1)
            assert not between.strip(), 'intervening source in definition domain'
            order = ('v34FreezeEcho', 'txrxdmainit', 'V34scrambler', 'V34SetupDemodulator') if variant == 'freeze-dma' else ('V34SetupDemodulator', 'v34FreezeEcho', 'V34scrambler', 'txrxdmainit')
            source = source[:begin] + '\n'.join(blocks[fn] for fn in order) + source[end:]
        for path, text in [('V34hshak.c', source), ('include/dsplib/v34fsk.h', header), ('include/dsplib/v34hstx1_arms.h', arms)]:
            (cell / path).write_text(text)
        directory = '/work/' + variant
        cmd = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(directory) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, ['-I' + directory + '/include'] + flags + (['-fno-peephole2'] if variant == 'no-peephole2' else []) + ['-da'], directory + '/V34hshak.o', directory + '/V34hshak.c')]
        entry = {'command': cmd, 'input_hashes': {p: hashlib.sha256((cell / p).read_bytes()).hexdigest() for p in ('V34hshak.c','include/dsplib/v34fsk.h','include/dsplib/v34hstx1_arms.h')}}
        results['cells'][variant] = entry
        with (cell / 'compile.log').open('w') as log:
            entry['compile_exit'] = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT).returncode
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        assert entry['compile_exit'] == 0, variant
        obj = str(cell / 'V34hshak.o')
        entry['object_hash'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals'] = {l.split()[-1]: l.split()[-2] for l in subprocess.check_output(['nm','-g','--defined-only',obj], text=True).splitlines()}
        entry['verdicts'] = {sym: b.verdict(*b.body(blob,sym),*b.body(obj,sym)) for sym in sorted(set(b.sizes(obj)) & set(b.sizes(blob)))}
        (cell / 'v34FreezeEcho.dis').write_text(subprocess.check_output(['objdump','-dr','--disassemble=v34FreezeEcho',obj], text=True))
        if variant == 'baseline':
            assert Path(obj).read_bytes() == prior_bytes, 'unchanged object drift'
            entry['raw_baseline_reproduced'] = True
        else:
            base = str(out / 'baseline/V34hshak.o')
            entry['changed_bodies'] = [sym for sym in sorted(set(b.sizes(obj)) & set(b.sizes(base))) if b.body(obj,sym) != b.body(base,sym)]
            entry['added_symbols'] = sorted(set(b.sizes(obj)) - set(b.sizes(base)))
            entry['removed_symbols'] = sorted(set(b.sizes(base)) - set(b.sizes(obj)))
            assert entry['globals'] == results['cells']['baseline']['globals']
        print(variant, 'freeze',entry['verdicts']['v34FreezeEcho'], 'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()), '/',len(entry['verdicts']), 'globals',len(entry['globals']),flush=True)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')

if __name__ == '__main__':
    main()
