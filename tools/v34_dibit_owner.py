#!/usr/bin/env python3
"""Five full-TU transmitter-owner/state boundary controls; compiler comparison only."""
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
           'seg_symcount': 'tx.seg_symcount', 'tx_flags': 'tx.flags',
           'prev_quadrant': 'tx.prev_quadrant', 'cur_quadrant': 'tx.cur_quadrant',
           'tx_scr_sr': 'tx.scr_sr', 'txpoint': 'tx.point'}

def migrate(text):
    for old, new in MEMBERS.items():
        text = re.sub(r'(->|\.)' + old + r'\b', lambda m: m[1] + new, text)
    for old, new in MEMBERS.items():
        text = re.sub(r'(V34HS_OFF\([^\n]*struct v34_object,\s*)' + old + r'\b',
                      lambda m: m[1] + new, text)
    return text

def owner_header(header):
    start = header.index('\tstruct v34_queue txq;')
    end = header.index('\n', header.index('\t} txpoint;', start))
    prefix = header[start:end]
    prefix = prefix.replace('struct v34_queue txq;', 'struct v34_queue queue;')
    prefix = prefix.replace('txq_ring_tail[', 'ring_tail[')
    prefix = prefix.replace('short tx_flags;', 'short flags;').replace('int tx_scr_sr;', 'int scr_sr;').replace('} txpoint;', '} point;')
    prefix = prefix.replace('/* +0x221c */', '/* +0x000 */')
    prefix = prefix.replace('/* to +0x25c0 */', '/* +0x010 to +0x3a4 */')
    prefix = prefix.replace('/* +0x25c0 */', '/* +0x3a4 */')
    prefix = prefix.replace('/* +0x25c2 */', '/* +0x3a6 */')
    header = header[:start] + '\tstruct v34_transmitter_prefix tx;\t/* +0x221c to +0x25d4 */' + header[end:]
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
    (sizeof(struct v34_transmitter_prefix) == 0x3b8) ? 1 : -1];
#endif

'''
    return header.replace('struct v34_object {', declaration + 'struct v34_object {', 1)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--revision', default='d884e844')
    ap.add_argument('--width-study', action='store_true')
    ap.add_argument('--cursor-study', action='store_true')
    args = ap.parse_args()
    out = ROOT / ('build/v34-dibit-cursor' if args.cursor_study else 'build/v34-dibit-width' if args.width_study else 'build/v34-dibit-owner')
    out.mkdir(parents=True, exist_ok=True)
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    prior = ROOT / ('build/v34-dibit-width/d-int-q-short/V34hshak.o' if args.cursor_study else 'build/v34-dibit-owner/owner-local-direct-root/V34hshak.o' if args.width_study else 'build/v34-dibit-owner/baseline/V34hshak.o' if (ROOT/'build/v34-dibit-owner/baseline/V34hshak.o').exists() else 'build/tc_out/src_pump_v34_V34hshak.c.o')
    prior_bytes = prior.read_bytes()
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')
    results = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5934798108', 'revision': args.revision, 'config': config, 'cells': {}}
    if args.width_study:
        results['domain'] = 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5934899273'
    if args.cursor_study:
        results['domain'] = 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5935011485'
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    def saved(path):
        return subprocess.check_output(['git', 'show', args.revision + ':' + path], cwd=ROOT, text=True)
    variants = ('baseline', 'd-int-q-short', 'd-short-q-int', 'd-short-q-short') if args.width_study else ('baseline', 'owner-root', 'owner-local-cached', 'owner-local-direct-root', 'owner-local-direct-owner')
    if args.cursor_study:
        variants = ('baseline', 'quad-before-dibit', 'no-peephole2')
    for variant in variants:
        cell = out / variant
        (cell / 'include/dsplib').mkdir(parents=True, exist_ok=True)
        source = saved('src/pump/v34/V34hshak.c')
        header = saved('include/dsplib/v34fsk.h')
        arms = saved('include/dsplib/v34hstx1_arms.h')
        if variant != 'baseline' or args.width_study or args.cursor_study:
            header = owner_header(header)
            source, arms = migrate(source), migrate(arms)
            if variant != 'owner-root':
                start = source.index('void\ntxmitdibit(')
                end = source.index('\n}\n', start) + 2
                fn = source[start:end]
                decl = '\tstruct v34_transmitter_prefix *tx = &o->tx;\n'
                anchor = '\tstruct v34_object *o = (struct v34_object *)obj;\n'
                assert fn.count(anchor) == 1
                fn = fn.replace(anchor, anchor + decl, 1)
                fn = fn.replace('tx_scrambler_mode(o)', '(short)((tx->flags & V34_TXFLAG_CALLER) == 0)')
                for field in ('prev_quadrant', 'cur_quadrant', 'point'):
                    fn = fn.replace('o->tx.' + field, 'tx->' + field)
                if 'direct' in variant or args.width_study or args.cursor_study:
                    fn = fn.replace('\tunsigned sr = (unsigned)o->tx.scr_sr;\n', '')
                    target = 'o->tx.scr_sr' if variant.endswith('root') or args.width_study or args.cursor_study else 'tx->scr_sr'
                    fn = fn.replace('V34scrambler(&sr,', 'V34scrambler((unsigned *)&' + target + ',')
                    fn = fn.replace('\to->tx.scr_sr = (int)sr;\n', '')
                if args.width_study and variant != 'baseline':
                    d_type = 'short' if variant.startswith('d-short') else 'int'
                    q_type = 'short' if variant.endswith('q-short') else 'int'
                    fn = fn.replace('\tint d, q;', '\t' + d_type + ' d;\n\t' + q_type + ' q;')
                if args.cursor_study:
                    fn = fn.replace('\tint d, q;', '\tint d;\n\tshort q;')
                source = source[:start] + fn + source[end:]
            source += '\n#if __SIZEOF_POINTER__ == 4\ntypedef char v34tx_root_point[(__builtin_offsetof(struct v34_object, tx.point) == 0x25d0) ? 1 : -1];\n#endif\n'
        if args.cursor_study and variant == 'quad-before-dibit':
            starts, ends, blocks = [], [], {}
            for name in ('txmitdibit','txmitquadbit'):
                fn_start = source.index('void\n'+name+'(')
                begin = source.rfind('\n/*',0,fn_start)
                end = source.index('\n}\n',fn_start)+3
                starts.append(begin);ends.append(end);blocks[name]=source[begin:end]
            begin,end=min(starts),max(ends)
            remaining=source[begin:end]
            for block in blocks.values():remaining=remaining.replace(block,'',1)
            assert not remaining.strip()
            source=source[:begin]+blocks['txmitquadbit']+'\n'+blocks['txmitdibit']+source[end:]
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
        (cell / 'txmitdibit.dis').write_text(subprocess.check_output(['objdump','-dr','--disassemble=txmitdibit',obj], text=True))
        if variant == 'baseline':
            assert Path(obj).read_bytes() == prior_bytes, 'unchanged object drift'
            entry['raw_baseline_reproduced'] = True
        else:
            base = str(out / 'baseline/V34hshak.o')
            entry['changed_bodies'] = [sym for sym in sorted(set(b.sizes(obj)) & set(b.sizes(base))) if b.body(obj,sym) != b.body(base,sym)]
            entry['added_symbols'] = sorted(set(b.sizes(obj)) - set(b.sizes(base)))
            entry['removed_symbols'] = sorted(set(b.sizes(base)) - set(b.sizes(obj)))
            assert entry['globals'] == results['cells']['baseline']['globals']
        print(variant, 'dibit',entry['verdicts']['txmitdibit'], 'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()), '/',len(entry['verdicts']), 'globals',len(entry['globals']),flush=True)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')

if __name__ == '__main__':
    main()
