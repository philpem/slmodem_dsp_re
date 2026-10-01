#!/usr/bin/env python3
"""Nine full-TU power owner/carrier controls; no fuzzing or mutation."""
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import experiment_toolchain as tc
import v34_dibit_owner as owner
import v34_power_reload as reload
sys.path.insert(0, str(ROOT/'tools/toolchain'))
import byteident as b


def power_header(header):
    header = owner.owner_header(header)
    start = header.index('\tshort tx_scale;')
    end = header.index('\tunsigned char unmapped_25de', start)
    fields = header[start:end].replace('short tx_scale;', 'short scale;').replace('short tx_pwr_reduction;', 'short pwr_reduction;')
    header = header[:start] + header[end:]
    start = header.index('struct v34_transmitter_prefix {')
    end = header.index('\n};', start)
    header = header[:end] + '\n' + fields + header[end:]
    header = header.replace('unmapped_25de[0x2a54 - 0x25de]', 'unmapped_25e0[0x2a54 - 0x25e0]')
    header = header.replace('== 0x3b8)', '== 0x3c4)').replace('/* +0x221c to +0x25d4 */', '/* +0x221c to +0x25e0 */')
    checks = '''
#if __SIZEOF_POINTER__ == 4
typedef char power_owner_scale[(__builtin_offsetof(struct v34_transmitter_prefix, scale) == 0x3b8) ? 1 : -1];
typedef char power_owner_reduction[(__builtin_offsetof(struct v34_transmitter_prefix, pwr_reduction) == 0x3c0) ? 1 : -1];
typedef char power_root_scale[(__builtin_offsetof(struct v34_object, tx.scale) == 0x25d4) ? 1 : -1];
typedef char power_root_reduction[(__builtin_offsetof(struct v34_object, tx.pwr_reduction) == 0x25dc) ? 1 : -1];
typedef char power_root_scrambler[(__builtin_offsetof(struct v34_object, scrambler) == 0x2a54) ? 1 : -1];
typedef char power_root_extent[(sizeof(struct v34_object) == 0xac4c) ? 1 : -1];
#endif
'''
    end = header.rfind('#endif')
    return header[:end] + checks + header[end:]


def migrate(text):
    text = owner.migrate(text)
    for old, new in [('tx_scale','tx.scale'), ('tx_pwr_reduction','tx.pwr_reduction')]:
        text = re.sub(r'(->|\.)' + old + r'\b', lambda m: m[1]+new, text)
        text = re.sub(r'(V34HS_OFF\([^\n]*struct v34_object,\s*)'+old+r'\b', lambda m: m[1]+new, text)
    return text


def main():
    out = ROOT/'build/v34-power-carrier'; out.mkdir(parents=True, exist_ok=True)
    revision = 'b57597e5'
    def saved(name):
        return subprocess.check_output(['git','show',revision+':'+name], cwd=ROOT, text=True)
    source = saved('src/pump/v34/V34hshak.c')
    header = saved('include/dsplib/v34fsk.h')
    arms = saved('include/dsplib/v34hstx1_arms.h')
    config = (ROOT/'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ',1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else '/src/'+x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    variants = ['baseline'] + [f'{layout}-{width}-{assignment}' for layout in ('flat','owner') for width in ('signed','unsigned') for assignment in ('before','branch')]
    results = {'revision':revision, 'config':config, 'domain':'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5936171161', 'cells':{}}
    tc.print_identity(image,tc.GENTOO_COMPILER_PATH,True)
    blob = str(ROOT/'ref/slmodemd/dsplibs.o')
    base = str(ROOT/'build/v34-power-reload/baseline/V34hshak.o')
    for variant in variants:
        cell = out/variant; (cell/'include/dsplib').mkdir(parents=True,exist_ok=True)
        text, h, arm = source, header, arms
        if variant != 'baseline':
            layout, width, assignment = variant.split('-')
            if layout == 'owner':
                text, h, arm = migrate(text), power_header(header), migrate(arms)
            start = text.index('void\nsettxlevel('); end = text.index('\n}\n',start)+2
            fn = reload.replacement(text[start:end])
            request = 'obj->tx.pwr_reduction' if layout == 'owner' else 'obj->tx_pwr_reduction'
            if layout == 'owner':
                fn = fn.replace('\tstruct v34_object *obj = (struct v34_object *)objp;', '\tstruct v34_object *obj = (struct v34_object *)objp;\n\tstruct v34_transmitter_prefix *tx = &obj->tx;')
                fn = fn.replace(request,'tx->pwr_reduction').replace('*(short *)(m + 0x25d4)','tx->scale').replace('\tunsigned char *m = (unsigned char *)obj;\n','')
                request = 'tx->pwr_reduction'
            if assignment == 'branch':
                old = '\twant = '+request+';\n\n\tif (want < 0) {'
                assert fn.count(old) == 1
                fn = fn.replace(old, '\tif ('+request+' < 0) {\n\t\twant = '+request+';')
                fn = fn.replace('\t} else {\n\t\tfor (n = 0; want > n;', '\t} else {\n\t\twant = '+request+';\n\t\tfor (n = 0; want > n;')
            if width == 'unsigned':
                fn = fn.replace('\tshort want;', '\tunsigned short want;')
                fn = fn.replace('if (want < 0)', 'if ((short)want < 0)').replace('n = want;', 'n = (short)want;').replace('want > n;', '(short)want > n;').replace('(int)want,', '(int)(short)want,')
            text = text[:start]+fn+text[end:]
        for name, data in [('V34hshak.c',text),('include/dsplib/v34fsk.h',h),('include/dsplib/v34hstx1_arms.h',arm)]:
            (cell/name).write_text(data)
        directory = '/work/'+variant
        cmd = tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(directory)+' && '+tc.compile_shell(tc.GENTOO_COMPILER_PATH,['-I'+directory+'/include']+flags+['-da'],directory+'/V34hshak.o',directory+'/V34hshak.c')]
        entry = {'command':cmd, 'input_hashes':{name:hashlib.sha256((cell/name).read_bytes()).hexdigest() for name in ('V34hshak.c','include/dsplib/v34fsk.h','include/dsplib/v34hstx1_arms.h')}}
        results['cells'][variant] = entry
        with (cell/'compile.log').open('w') as log:
            entry['compile_exit'] = subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
        assert entry['compile_exit'] == 0, variant
        obj = str(cell/'V34hshak.o')
        entry['object_hash'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals'] = {line.split()[-1]:line.split()[-2] for line in subprocess.check_output(['nm','-g','--defined-only',obj],text=True).splitlines()}
        entry['function_symbols'] = sorted(b.sizes(obj))
        entry['verdicts'] = {sym:b.verdict(*b.body(blob,sym),*b.body(obj,sym)) for sym in sorted(set(b.sizes(obj))&set(b.sizes(blob)))}
        entry['changed_bodies'] = [sym for sym in sorted(set(b.sizes(obj))&set(b.sizes(base))) if b.body(obj,sym)!=b.body(base,sym)]
        (cell/'settxlevel.dis').write_text(subprocess.check_output(['objdump','-dr','--disassemble=settxlevel',obj],text=True))
        if variant == 'baseline':
            assert Path(obj).read_bytes()==Path(base).read_bytes(), 'raw full-TU drift'
            entry['baseline_reproduced'] = True
        else:
            control = results['cells']['baseline']
            assert entry['globals']==control['globals']
            assert entry['function_symbols']==control['function_symbols']
        print(variant,entry['verdicts']['settxlevel'],'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),'/',len(entry['verdicts']),flush=True)
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__ == '__main__':
    main()
