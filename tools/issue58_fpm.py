#!/usr/bin/env python3
"""Bounded full-TU issue58 experiment; run from this worktree's root."""
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.remove(str(ROOT / 'tools'))
import dis  # prevent tools/dis.py shadowing stdlib
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import experiment_toolchain as tc
import byteident as bi
import subprocess, shlex, json, hashlib, itertools, difflib
from collections import Counter
from elftools.elf.elffile import ELFFile

OUT = ROOT / 'build/issue58-fpm'
IMAGE = 'ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest'
FLAGS = [f for f in shlex.split((ROOT / 'build/issue58-valid-make/.build-config').read_text().splitlines()[1][6:])
         if f != tc.REPRODUCE_BUGS]

def run(cmd):
    return subprocess.run(cmd, check=True, text=True, capture_output=True).stdout

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def score(obj):
    names = bi.sizes(str(obj))
    shared = sorted(names.keys() & bi.sizes(bi.BLOB).keys())
    assert shared
    return {s: bi.verdict(*bi.body(bi.BLOB, s), *bi.body(str(obj), s)) for s in shared}

def inventory(obj):
    with open(obj, 'rb') as stream:
        elf = ELFFile(stream)
        symbols = []
        for s in elf.get_section_by_name('.symtab').iter_symbols():
            if s['st_info']['type'] in ('STT_FILE','STT_SECTION') or not s.name:
                continue
            idx = s['st_shndx']
            sec = elf.get_section(idx).name if isinstance(idx, int) else idx
            symbols.append([s.name, s['st_info']['type'], s['st_info']['bind'],
                            s['st_other']['visibility'], sec])
        data = {s.name: [s['sh_size'], s['sh_addralign'], hashlib.sha256(s.data()).hexdigest()]
                for s in elf.iter_sections() if s['sh_flags'] & 2 and not s['sh_flags'] & 4}
        return sorted(symbols), data

def audit():
    convergence=[]
    for tu in ['sre','fse']:
        for profile in ['O2','no-sched2']:
            a=OUT/'matrix'/(tu+'-assign-'+profile)/('fpm_'+tu+'.o')
            b=OUT/'matrix'/(tu+'-memcpy-'+profile)/('fpm_'+tu+'.o')
            convergence.append(dict(tu=tu, profile=profile, object_equal=a.read_bytes()==b.read_bytes(),
                bodies={s:bi.verdict(*bi.body(str(a),s),*bi.body(str(b),s)) for s in bi.sizes(str(a))}))
    print('Copy-form comparisons:',convergence)
    (OUT/'copy-form-comparison.json').write_text(json.dumps(convergence,indent=2))
    rows = []
    for batch in ['control','matrix','orders']:
        records = json.loads((OUT / batch / 'results.json').read_text())
        for m in records:
            obj = Path(m['object'])
            base = OUT / 'control' / ('baseline-' + m['tu']) / obj.name
            syms, data = inventory(obj)
            bsyms, bdata = inventory(base)
            shared = sorted(m['scores'])
            exact = [s for s,v in m['scores'].items() if v[0] == 'EXACT']
            bexact = [s for s,v in score(base).items() if v[0] == 'EXACT']
            rows.append(dict(cell=m['name'], denominator=len(shared),
                gain=sorted(set(exact)-set(bexact)), loss=sorted(set(bexact)-set(exact)),
                symbol_inventory_changed=syms != bsyms, allocated_nontext_changed=data != bdata,
                changed=[s for s,v in m['vs_baseline'].items() if v[0] != 'EXACT'],
                relocation_target_multiset_changed=[s for s in shared if
                    Counter(bi.body(str(base),s)[1].values()) != Counter(bi.body(str(obj),s)[1].values())],
                canonical_relocations_changed=[s for s in shared if bi.body(str(base),s)[1] != bi.body(str(obj),s)[1]]))
            if syms != bsyms:
                print(m['name'],'inventory added',[s for s in syms if s not in bsyms],
                      'removed',[s for s in bsyms if s not in syms])
    (OUT / 'audit.json').write_text(json.dumps(rows, indent=2))
    print('Audit cells', len(rows), 'shared function verdicts', sum(x['denominator'] for x in rows),
          'binding/inventory changes',sum(x['symbol_inventory_changed'] for x in rows),
          'allocated nontext changes',sum(x['allocated_nontext_changed'] for x in rows))
    print('Relocation target-multiset changes',[(r['cell'],r['relocation_target_multiset_changed']) for r in rows if r['relocation_target_multiset_changed']])
    print('Order collateral', sorted({tuple(s for s in r['changed'] if not s.endswith('_init')) for r in rows if '-order-' in r['cell']}))
    for tu in ['sre','fse']:
        records = json.loads((OUT / 'orders/results.json').read_text())
        rows2 = [r for r in records if r['tu']==tu]
        target='FPM_'+tu.upper()+'_init'
        hits=[r for r in rows2 if r['scores'][target][0]=='EXACT']
        print(tu, 'distinct target bodies',len({bi.body(r['object'],target)[0] for r in rows2}),
              'exact object hashes',sorted({r['object_sha256'] for r in hits}))
        if tu=='sre':
            allslots=list(itertools.combinations(range(8),3))
            hit_slots=[allslots[int(r['name'].rsplit('-',1)[1])] for r in hits]
            common=[]
            for a,b in itertools.permutations(['active','acquiring','adapt','mode','pll_acc','err_avg','mag_avg','taps'],2):
                def order(slots):
                    x=iter(['active','acquiring','adapt']); y=iter(['mode','pll_acc','err_avg','mag_avg','taps'])
                    return [next(x) if i in slots else next(y) for i in range(8)]
                if all(order(s).index(a)<order(s).index(b) for s in hit_slots):
                    common.append([a,b])
            print('sre common precedence', common)
            (OUT/'sre-common-precedence.json').write_text(json.dumps(common,indent=2))
            for r, slots in zip(rows2, allslots):
                assert (r['scores'][target][0]=='EXACT') == (order(slots).index('mag_avg') < order(slots).index('adapt'))
            print('SRE exact iff mag_avg precedes adapt: 56/56 cells')
    # Preserve complete dis.py readings for each distinct experimental object.
    seen=set()
    for batch in ['control','matrix','orders']:
        for m in json.loads((OUT/batch/'results.json').read_text()):
            if m['object_sha256'] in seen:
                continue
            seen.add(m['object_sha256'])
            obj=Path(m['object'])
            text=[]
            for sym in sorted(bi.sizes(str(obj))):
                text.append('### '+sym+'\n'+run([sys.executable,'tools/dis.py',str(obj),sym]))
            (obj.parent/'disassembly.txt').write_text('\n'.join(text))
    for tu in ['sre','fse']:
        base=OUT/'control'/('baseline-'+tu)/('fpm_'+tu+'.o')
        text=[]
        for sym in score(base):
            text.append('### '+sym+'\n'+run([sys.executable,'tools/dis.py',bi.BLOB,sym]))
        (OUT/('reference-'+tu+'.txt')).write_text('\n'.join(text))
    reference_symbols,_=inventory(bi.BLOB)
    shared_names=set()
    for tu in ['sre','fse']:
        base=OUT/'control'/('baseline-'+tu)/('fpm_'+tu+'.o')
        shared_names.update(score(base))
    shared_bindings=[s for s in reference_symbols if s[0] in shared_names]
    assert len(shared_bindings)==7
    print('Reference shared function binding inventory:',shared_bindings)
    (OUT/'reference-identity.json').write_text(json.dumps({'blob':bi.BLOB,
        'sha256':sha(Path(bi.BLOB)), 'base_revision':run(['git','rev-parse','HEAD']).strip(),
        'shared_function_bindings':shared_bindings,
        'apparatus_sha256':sha(Path(__file__)),
        'byteident_sha256':sha(ROOT/'tools/toolchain/byteident.py'),
        'helper_sha256':sha(ROOT/'tools/experiment_toolchain.py')},indent=2))

def compile_cells(cells, directory):
    directory.mkdir(parents=True, exist_ok=True)
    commands = []
    manifest = []
    for name, tu, source, extra in cells:
        folder = directory / name
        folder.mkdir(exist_ok=True)
        src = folder / ('fpm_' + tu + '.c')
        src.write_text(source)
        obj = folder / ('fpm_' + tu + '.o')
        cs = '/work/' + str(src.relative_to(OUT))
        co = '/work/' + str(obj.relative_to(OUT))
        flags=FLAGS + extra
        if '-da' in extra:
            flags=['-I/src/include' if f=='-Iinclude' else '/src/tools/toolchain/period_compat.h'
                   if f=='tools/toolchain/period_compat.h' else f for f in flags]
        command = tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags, co, cs)
        if '-da' in extra:
            prefix=tc.docker_prefix(IMAGE,ROOT,OUT,True)
            prefix[prefix.index('-w')+1]='/work/'+str(folder.relative_to(OUT))
            commands.append(shlex.join(prefix+['/bin/sh','-c',command]))
        else:
            commands.append('/bin/sh -c ' + shlex.quote(command))
        manifest.append(dict(name=name, tu=tu, source=str(src), object=str(obj),
                             source_sha256=sha(src), command=command,
                             local_headers={str(p.relative_to(folder)):sha(p) for p in (folder/'dsplib').glob('*.h')}))
    script = directory / 'compile.sh'
    script.write_text('set -eu\n' + '\n'.join(commands) + '\n')
    (directory / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    if any('-da' in cell[3] for cell in cells):
        cmd=['/bin/sh',str(script)]
    else:
        cmd = tc.docker_prefix(IMAGE, ROOT, OUT, True) + ['/bin/sh', '/work/' + str(script.relative_to(OUT))]
    result = subprocess.run(cmd, text=True, capture_output=True)
    (directory / 'compile.log').write_text(shlex.join(cmd) + '\n' + result.stdout + result.stderr)
    result.check_returncode()
    records = []
    for m in manifest:
        obj = Path(m['object'])
        m['object_sha256'] = sha(obj)
        m['scores'] = score(obj)
        base = OUT / 'control' / ('baseline-' + m['tu']) / obj.name
        m['vs_baseline'] = {s: bi.verdict(*bi.body(str(base), s), *bi.body(str(obj), s))
                            for s in m['scores']}
        for args, suffix in [(['readelf', '-sWr'], 'symbols-relocs.txt'),
                             (['readelf', '-SW'], 'sections.txt')]:
            (obj.parent / suffix).write_text(run(args + [str(obj)]))
        bodies = {s: [bi.body(str(obj), s)[0].hex(), bi.body(str(obj), s)[1]] for s in m['scores']}
        (obj.parent / 'bodies.json').write_text(json.dumps(bodies, indent=2, default=lambda b: {'bytes_hex': b.hex()}))
        records.append(m)
    (directory / 'results.json').write_text(json.dumps(records, indent=2))
    if len(records) < 20:
        for m in records:
            print(m['name'], m['scores'], 'changed', [s for s,v in m['vs_baseline'].items() if v[0] != 'EXACT'])
    else:
        for tu in sorted({m['tu'] for m in records}):
            rows = [m for m in records if m['tu'] == tu]
            sym = 'FPM_' + tu.upper() + '_init'
            print(tu, 'compiled', len(rows), 'exact hits', [m['name'] for m in rows if m['scores'][sym][0] == 'EXACT'],
                  'distinct objects', len({m['object_sha256'] for m in rows}))

def fse_audit():
    """Continuation-only report: never rewrite the earlier SRE evidence packet."""
    directory=OUT/'fse-continuation-audit'
    directory.mkdir(exist_ok=True)
    records=[]
    for batch in ['fse-rtl-v2','fse-enables','fse-types','fse-type-rtl']:
        p=OUT/batch/'results.json'
        if p.exists():
            records+=json.loads(p.read_text())
    base=OUT/'control/baseline-fse/fpm_fse.o'
    bsyms,bdata=inventory(base)
    baseline_scores=score(base)
    bexact={s for s,v in baseline_scores.items() if v[0]=='EXACT'}
    rows=[]
    seen={}
    for m in records:
        obj=Path(m['object'])
        syms,data=inventory(obj)
        exact={s for s,v in m['scores'].items() if v[0]=='EXACT'}
        assert set(m['scores'])==set(baseline_scores)
        assert len(m['scores'])==4
        rows.append(dict(cell=m['name'],sha256=m['object_sha256'],
            gain=sorted(exact-bexact),loss=sorted(bexact-exact),
            inventory_changed=syms!=bsyms,allocated_nontext_changed=data!=bdata,
            changed=[s for s,v in m['vs_baseline'].items() if v[0]!='EXACT'],
            relocation_sites_changed=[s for s in m['scores'] if bi.body(str(base),s)[1]!=bi.body(str(obj),s)[1]],
            relocation_targets_changed=[s for s in m['scores'] if Counter(bi.body(str(base),s)[1].values())!=Counter(bi.body(str(obj),s)[1].values())]))
        if m['object_sha256'] not in seen:
            seen[m['object_sha256']]=m['object']
            (obj.parent/'disassembly.txt').write_text('\n'.join('### '+s+'\n'+run([sys.executable,'tools/dis.py',str(obj),s]) for s in sorted(bi.sizes(str(obj)))))
    (directory/'audit.json').write_text(json.dumps(rows,indent=2))
    comparisons=[]
    for pos in ['first','after-enable']:
        for form in ['assign','builtin','library']:
            a=OUT/'fse-types'/('fse-types-%s-%s-int-zero'%(form,pos))/'fpm_fse.o'
            b=OUT/'fse-types'/('fse-types-%s-%s-int-longzero'%(form,pos))/'fpm_fse.o'
            comparisons.append([form,pos,'literal',a.read_bytes()==b.read_bytes()])
        for typ in ['int-zero','int-longzero','long-zero']:
            a=OUT/'fse-types'/('fse-types-builtin-%s-%s'%(pos,typ))/'fpm_fse.o'
            b=OUT/'fse-types'/('fse-types-library-%s-%s'%(pos,typ))/'fpm_fse.o'
            comparisons.append([pos,typ,'library-vs-builtin',a.read_bytes()==b.read_bytes()])
    assert all(x[-1] for x in comparisons)
    assert (OUT/'fse-types/fse-types-assign-first-int-zero/fpm_fse.o').read_bytes()==base.read_bytes()
    (directory/'controls.json').write_text(json.dumps(comparisons,indent=2))
    (directory/'identity.json').write_text(json.dumps(dict(base_revision=run(['git','rev-parse','HEAD']).strip(),
        apparatus_sha256=sha(Path(__file__)),flags=tc.add_reproduce_bugs(FLAGS),
        source_sha256=sha(ROOT/'src/dsp/fpm_fse.c'),header_sha256=sha(ROOT/'include/dsplib/fpm_fse.h'),
        image=IMAGE,image_identity=run(['docker','image','inspect',IMAGE,'--format','{{.Id}} {{json .RepoDigests}}']).strip(),blob_sha256=sha(Path(bi.BLOB)),
        distinct_object_representatives=seen),indent=2))
    intact=0
    for batch in ['control','matrix','orders']:
        for m in json.loads((OUT/batch/'results.json').read_text()):
            if m['tu']=='sre':
                assert sha(Path(m['source']))==m['source_sha256']
                assert sha(Path(m['object']))==m['object_sha256']
                intact+=2
    (directory/'sre-integrity.txt').write_text('%d/%d prior SRE source/object hashes intact\n'%(intact,intact))
    print('Continuation audit:',len(rows),'cells;',len(rows)*4,'shared-function verdicts;',len(seen),'distinct objects')
    print('Exact hits:',[r['cell'] for r in rows if 'FPM_FSE_init' in r['gain']])
    print('Losses:',[(r['cell'],r['loss']) for r in rows if r['loss']])
    print('Non-init collateral:',[(r['cell'],r['changed']) for r in rows if any(s!='FPM_FSE_init' for s in r['changed'])])
    print('Inventory/nontext/reloc-target changes:',[(r['cell'],r['inventory_changed'],r['allocated_nontext_changed'],r['relocation_targets_changed']) for r in rows if r['inventory_changed'] or r['allocated_nontext_changed'] or r['relocation_targets_changed']])
    print('Controls: baseline overlay 1/1 exact;',len(comparisons),'literal/library comparisons exact')
    print('Prior SRE artifact hashes intact:',intact,'/',intact)

def fse_fresh_audit():
    directory=OUT/'fse-fresh-audit'
    directory.mkdir(exist_ok=True)
    records=json.loads((OUT/'fse-fresh/results.json').read_text())
    assert len(records)==20
    base=OUT/'control/baseline-fse/fpm_fse.o'
    bsyms,bdata=inventory(base)
    rows=[]
    for m in records:
        obj=Path(m['object'])
        syms,data=inventory(obj)
        assert set(m['scores'])==set(score(base))
        exact={s for s,v in m['scores'].items() if v[0]=='EXACT'}
        rows.append(dict(cell=m['name'],score=m['scores']['FPM_FSE_init'],object_sha256=sha(obj),
            init_size=bi.sizes(str(obj))['FPM_FSE_init'],
            gain=sorted(exact-{'FPM_FSE_free'}),loss=sorted({'FPM_FSE_free'}-exact),
            changed=[s for s,v in m['vs_baseline'].items() if v[0]!='EXACT'],
            inventory_changed=syms!=bsyms,allocated_nontext_changed=data!=bdata,
            relocation_sites_changed=[s for s in m['scores'] if bi.body(str(base),s)[1]!=bi.body(str(obj),s)[1]],
            relocation_targets_changed=[s for s in m['scores'] if Counter(bi.body(str(base),s)[1].values())!=Counter(bi.body(str(obj),s)[1].values())]))
        (obj.parent/'disassembly.txt').write_text('\n'.join('### '+s+'\n'+run([sys.executable,'tools/dis.py',str(obj),s]) for s in sorted(bi.sizes(str(obj)))))
    comparisons=[]
    for copyform in ['assign','library']:
        for position in ['first','after-enable']:
            def current(form):
                return OUT/'fse-fresh'/('fse-fresh-%s-%s-%s'%(copyform,position,form))/'fpm_fse.o'
            prior=OUT/'fse-types'/('fse-types-%s-%s-int-zero'%(copyform,position))/'fpm_fse.o'
            comparisons.append([copyform,position,'prior control',prior.read_bytes()==current('int').read_bytes()])
            for form in ['receive-long','receive-ulong']:
                comparisons.append([copyform,position,form,current('int').read_bytes()==current(form).read_bytes()])
            comparisons.append([copyform,position,'long-vs-ulong',current('long').read_bytes()==current('ulong').read_bytes()])
    assert all(c[-1] for c in comparisons)
    phase_probe=OUT/'fse-types/fse-types-library-after-enable-long-zero/fpm_fse.o'
    arg_probe=OUT/'fse-fresh/fse-fresh-library-after-enable-long/fpm_fse.o'
    equivalence=phase_probe.read_bytes()==arg_probe.read_bytes()
    intact=0
    for batch in ['control','matrix','orders']:
        for m in json.loads((OUT/batch/'results.json').read_text()):
            if m['tu']=='sre':
                assert sha(Path(m['source']))==m['source_sha256']
                assert sha(Path(m['object']))==m['object_sha256']
                intact+=2
    (directory/'audit.json').write_text(json.dumps(rows,indent=2))
    (directory/'controls.json').write_text(json.dumps(dict(comparisons=comparisons,
        prior_long_phase_vs_long_arg_object_equal=equivalence,sre_artifact_hashes_intact=intact),indent=2))
    (directory/'identity.json').write_text(json.dumps(dict(base_revision=run(['git','rev-parse','HEAD']).strip(),
        source_sha256=sha(ROOT/'src/dsp/fpm_fse.c'),header_sha256=sha(ROOT/'include/dsplib/fpm_fse.h'),
        apparatus_sha256=sha(Path(__file__)),blob_sha256=sha(Path(bi.BLOB)),
        image_identity=run(['docker','image','inspect',IMAGE,'--format','{{.Id}} {{json .RepoDigests}}']).strip()),indent=2))
    for r in rows:
        print(r['cell'],r['score'],r['init_size'])
    print('Final batch: 20/20 compiled; 80 shared-function verdicts; exact gains/losses',[(r['cell'],r['gain'],r['loss']) for r in rows if r['gain'] or r['loss']])
    print('Other body/inventory/data/reloc-target changes',[(r['cell'],r) for r in rows if any(s!='FPM_FSE_init' for s in r['changed']) or r['inventory_changed'] or r['allocated_nontext_changed'] or r['relocation_targets_changed']])
    print('Controls:',len(comparisons),'/',len(comparisons),'whole-object comparisons equal; prior phase vs argument diagnostic equal:',equivalence)
    print('SRE hashes intact',intact)

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sources = {tu: (ROOT / ('src/dsp/fpm_' + tu + '.c')).read_text() for tu in ['sre', 'fse']}
    if sys.argv[1] == 'audit':
        audit()
    elif sys.argv[1] == 'fse-audit':
        fse_audit()
    elif sys.argv[1] == 'fse-fresh-audit':
        fse_fresh_audit()
    elif sys.argv[1] == 'control':
        with (OUT / 'identity.txt').open('w') as log:
            cmd = [sys.executable, '-c',
                   "import sys; sys.path.insert(0,'tools'); import experiment_toolchain as t; t.print_identity(%r,t.GENTOO_COMPILER_PATH,True)" % IMAGE]
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True)
            log.write(run(['docker', 'image', 'inspect', IMAGE, '--format', '{{.Id}} {{json .RepoDigests}}']))
        (OUT / 'inputs.json').write_text(json.dumps({str(p.relative_to(ROOT)): sha(p) for p in
            [ROOT / ('src/dsp/fpm_' + tu + '.c') for tu in sources] +
            list((ROOT / 'include').rglob('*.h')) + [ROOT / 'tools/toolchain/period_compat.h']}, indent=2))
        compile_cells([('baseline-' + t,t,s,[]) for t,s in sources.items()], OUT / 'control')
    elif sys.argv[1] == 'matrix':
        # Domain declared in docs/issue58-fpm.md BEFORE execution.
        cells = []
        for tu, src in sources.items():
            obj = 'sre' if tu == 'sre' else 'state'
            line = '\t' + obj + '->cfg = *cfg;'
            variants = {'assign':src, 'memcpy':src.replace(line,
                '\t__builtin_memcpy(&' + obj + '->cfg, cfg, sizeof(' + obj + '->cfg));')}
            assert variants['assign'] != variants['memcpy']
            for form, text in variants.items():
                for profile, extra in [('O3',[]),('no-sched2',['-fno-schedule-insns2']),('O2',['-O2'])]:
                    cells.append((tu+'-'+form+'-'+profile,tu,text,extra))
        compile_cells(cells, OUT / 'matrix')
    elif sys.argv[1] == 'orders':
        cells = []
        src = sources['sre']
        start = src.index('\tsre->active = 0;')
        end = src.index('\tsre->fill = 0;', start)
        lines = src[start:end].splitlines(True)
        assert len(lines) == 8
        for n, slots in enumerate(itertools.combinations(range(8), 3)):
            a, b = iter(lines[:3]), iter(lines[3:])
            block = ''.join(next(a) if i in slots else next(b) for i in range(8))
            cells.append(('sre-order-%03d'%n,'sre',src[:start]+block+src[end:],[]))
        src = sources['fse']
        start = src.index('\tstate->lms_force = 0;')
        end = src.index('\n\n', start)
        lines = src[start:end].splitlines(True)
        lines[-1] += '\n'
        assert len(lines) == 17
        freq = next(x for x in lines if '->freq =' in x)
        phase = next(x for x in lines if '->phase_acc =' in x)
        rest = [x for x in lines if x not in (freq, phase)]
        for n, (p, q) in enumerate(itertools.permutations(range(17), 2)):
            it = iter(rest)
            block = ''.join(freq if i == p else phase if i == q else next(it) for i in range(17)).rstrip('\n')
            cells.append(('fse-order-%03d'%n,'fse',src[:start]+block+src[end:],[]))
        assert len(cells) == 328
        compile_cells(cells, OUT / 'orders')
    elif sys.argv[1] == 'fse-rtl':
        src=sources['fse']
        variants={'assign':src,'memcpy':src.replace('\tstate->cfg = *cfg;',
            '\t__builtin_memcpy(&state->cfg, cfg, sizeof(state->cfg));')}
        cells=[]
        for form,text in variants.items():
            name='fse-rtl-'+form
            cells.append((name,'fse',text,['-da','-fsched-verbose=5']))
        compile_cells(cells, OUT/'fse-rtl-v2')
    elif sys.argv[1] == 'fse-enables':
        src=sources['fse']
        start=src.index('\tstate->cfg = *cfg;')
        end=src.index('\n\n',src.index('\tstate->lms_force = 0;',start))
        lines=src[start:end].splitlines()
        stores=[x for x in lines if x and '->cfg =' not in x]
        assert len(stores)==17
        block=stores[:4]
        rest=stores[4:]
        assert len(rest)==13
        cells=[]
        for form,copy in [('assign','\tstate->cfg = *cfg;'),
                          ('memcpy','\t__builtin_memcpy(&state->cfg, cfg, sizeof(state->cfg));')]:
            for position in ['first','after-enable']:
                for slot in range(14):
                    ordered=rest[:slot]+block+([copy] if position=='after-enable' else [])+rest[slot:]
                    if position=='first':
                        ordered=[copy]+ordered
                    text=src[:start]+'\n'.join(ordered)+src[end:]
                    cells.append(('fse-enables-%s-%s-%02d'%(form,position,slot),'fse',text,[]))
        assert len(cells)==56
        compile_cells(cells,OUT/'fse-enables')
    elif sys.argv[1] == 'fse-types':
        src=sources['fse']
        header=(ROOT/'include/dsplib/fpm_fse.h').read_text()
        original='\tint phase_acc;'
        assert header.count(original)==1
        copyline='\tstate->cfg = *cfg;'
        enable_end='\tstate->lms_on = 1;'
        cells=[]
        for form in ['assign','builtin','library']:
            for position in ['first','after-enable']:
                for typ in ['int-zero','int-longzero','long-zero']:
                    text=src
                    copy={'assign':copyline,
                          'builtin':'\t__builtin_memcpy(&state->cfg, cfg, sizeof(state->cfg));',
                          'library':'\tmemcpy(&state->cfg, cfg, sizeof(state->cfg));'}[form]
                    if form=='library':
                        text=text.replace('#include "dsplib/debug.h"','#include <string.h>\n#include "dsplib/debug.h"')
                    if position=='first':
                        text=text.replace(copyline,copy)
                    else:
                        text=text.replace(copyline+'\n\n','').replace(enable_end,enable_end+'\n\n'+copy)
                    if typ=='int-longzero':
                        text=text.replace('\tstate->phase_acc = 0;','\tstate->phase_acc = 0L;')
                    name='fse-types-%s-%s-%s'%(form,position,typ)
                    directory=OUT/'fse-types'/name/'dsplib'
                    directory.mkdir(parents=True,exist_ok=True)
                    overlay=header.replace(original,'\tlong phase_acc;') if typ=='long-zero' else header
                    (directory/'fpm_fse.h').write_text(overlay)
                    cells.append((name,'fse',text,[]))
        assert len(cells)==18
        compile_cells(cells,OUT/'fse-types')
    elif sys.argv[1] == 'fse-type-rtl':
        cells=[]
        for typ in ['int-zero','long-zero']:
            old='fse-types-library-after-enable-'+typ
            name='fse-type-rtl-'+typ
            source=OUT/'fse-types'/old
            directory=OUT/'fse-type-rtl'/name/'dsplib'
            directory.mkdir(parents=True,exist_ok=True)
            (directory/'fpm_fse.h').write_bytes((source/'dsplib/fpm_fse.h').read_bytes())
            cells.append((name,'fse',(source/'fpm_fse.c').read_text(),['-da','-fsched-verbose=5']))
        compile_cells(cells,OUT/'fse-type-rtl')
    elif sys.argv[1] == 'fse-fresh':
        src=sources['fse']
        header=(ROOT/'include/dsplib/fpm_fse.h').read_text()
        signature='FPM_FSE_init(struct fpm_fse *state, const struct fpm_fse_cfg *cfg, int fresh)'
        assert src.count(signature)==1 and header.count('int fresh);')==1
        copyline='\tstate->cfg = *cfg;'
        enable_end='\tstate->lms_on = 1;'
        forms=[('int','int',None),('long','long',None),('ulong','unsigned long',None),
               ('receive-long','int','long'),('receive-ulong','int','unsigned long')]
        cells=[]
        for copyform in ['assign','library']:
            for position in ['first','after-enable']:
                for argform,formal,receiving in forms:
                    text=src.replace(signature,signature.replace('int fresh)',formal+' fresh)'))
                    overlay=header.replace('int fresh);',formal+' fresh);')
                    if receiving:
                        sig=signature.replace('int fresh)',formal+' fresh)')
                        text=text.replace(sig+'\n{',sig+'\n{\n\t'+receiving+' fresh_value = fresh;')
                        assert text.count('if (!fresh)')==1
                        text=text.replace('if (!fresh)','if (!fresh_value)')
                    copy=copyline
                    if copyform=='library':
                        text=text.replace('#include "dsplib/debug.h"','#include <string.h>\n#include "dsplib/debug.h"')
                        copy='\tmemcpy(&state->cfg, cfg, sizeof(state->cfg));'
                    if position=='first':
                        text=text.replace(copyline,copy)
                    else:
                        text=text.replace(copyline+'\n\n','').replace(enable_end,enable_end+'\n\n'+copy)
                    name='fse-fresh-%s-%s-%s'%(copyform,position,argform)
                    directory=OUT/'fse-fresh'/name/'dsplib'
                    directory.mkdir(parents=True,exist_ok=True)
                    assert '\tint phase_acc;' in overlay
                    (directory/'fpm_fse.h').write_text(overlay)
                    cells.append((name,'fse',text,['-da','-fsched-verbose=5']))
        assert len(cells)==20
        compile_cells(cells,OUT/'fse-fresh')

if __name__ == '__main__':
    main()
