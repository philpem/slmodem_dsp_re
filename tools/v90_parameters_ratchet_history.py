#!/usr/bin/env python3
"""Historical TU/profile cross following the unchanged compiler raw control."""
import argparse,hashlib,json,shlex,subprocess
from pathlib import Path
import playbook_small_patterns as d
import experiment_toolchain as tc
from gcc3_value_carriers_audit import inspect
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/v90-parameters-ratchet-ab'
TARGET='_ZN13V90ParametersC2EP19_tagModemParameters';SOURCE='src/pump/v90/V90Parameters.cpp'
REV='bf40be37'

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--retype-era',action='store_true');ap.add_argument('--floor-stock',action='store_true');ap.add_argument('--dump-era',action='store_true');args=ap.parse_args()
    report=json.loads((OUT/'results.json').read_text());baseline=OUT/'gentoo/candidate.o'
    assert report['cells']['gentoo']['raw_baseline_reproduced']
    config=report['config'];image=config.splitlines()[0].split(' ',1)[1]
    common=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    common=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in common]
    current=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('cxx   ')))
    oldmake=subprocess.check_output(['git','show',REV+':tools/toolchain/period.mk'],cwd=ROOT,text=True)
    assert 'TC_CXXONLY := -fno-exceptions -fno-rtti\n' in oldmake
    paths=list(report['cells']['gentoo']['source_and_header_hashes'])
    paths=[p for p in paths if p!=SOURCE]
    assert not args.dump_era or args.retype_era
    matrix={}
    domains=[('pre-434-retype',True,False,'2efa968b^'),('post-434-retype',True,False,'2efa968b')] if args.retype_era else [('floor-tu-floor-profile',True,True,REV),('floor-tu-current-profile',True,False,REV),('current-tu-floor-profile',False,True,REV)]
    if args.floor_stock: domains=[('floor-tu-floor-profile-stock',True,True,REV)]
    if args.floor_stock: image='dsplibs-tc342'
    compiler_path='/opt/gcc342/bin' if args.floor_stock else tc.GENTOO_COMPILER_PATH
    for label,historical_source,historical_profile,historical_revision in domains:
        cell=OUT/label;cell.mkdir(exist_ok=True);dst='/work/'+label
        raw_before=(cell/'candidate.o').read_bytes() if args.dump_era else None
        if raw_before is not None:(cell/'raw-before-dumps.o').write_bytes(raw_before)
        source=subprocess.check_output(['git','show',(historical_revision if historical_source else '75e7ef4b')+':'+SOURCE],cwd=ROOT)
        (cell/'V90Parameters.cpp').write_bytes(source)
        flags=common.copy()
        if historical_source:
            for path in paths:
                file=cell/'historical'/path;file.parent.mkdir(parents=True,exist_ok=True)
                file.write_bytes(subprocess.check_output(['git','show',historical_revision+':'+path],cwd=ROOT))
            flags=['-I'+dst+'/historical/include' if x=='-I/src/include' else dst+'/historical/tools/toolchain/period_compat.h' if x=='/src/tools/toolchain/period_compat.h' else x for x in flags]
        if args.dump_era:flags+=['-v','-save-temps','-da','-dP']
        flags+=['-fno-exceptions','-fno-rtti'] if historical_profile else current
        shell=tc.compile_shell(compiler_path,flags+['-MMD','-MF',dst+'/dependencies.d'],dst+'/candidate.o',dst+'/V90Parameters.cpp').replace('exec gcc -c ','exec g++ -c ',1)
        command=tc.docker_prefix(image,ROOT,OUT,True)+['/bin/sh','-c','cd '+shlex.quote(dst)+' && '+shell]
        with (cell/'compile.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
        obj=cell/'candidate.o';headers={}
        if raw_before is not None:assert obj.read_bytes()==raw_before,'dump instrumentation changed raw object'
        deps=(cell/'dependencies.d').read_text().replace('\\\n',' ').split(':',1)[1].split()
        for dependency in deps:
            if dependency.startswith('/src/'):
                relative=dependency[5:]
                assert not historical_source,'historical header closure fell back to current: '+relative
                headers[relative]=hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()
            elif '/historical/' in dependency:
                relative=dependency.split('/historical/',1)[1]
                headers[relative]=hashlib.sha256((cell/'historical'/relative).read_bytes()).hexdigest()
        a=inspect(baseline);b=inspect(obj)
        entry={'command':command,'source_sha256':hashlib.sha256(source).hexdigest(),'headers':headers,
               'dump_raw_repeat':args.dump_era,'historical_revision':historical_revision,'historical_source':historical_source,'historical_cxx_profile':historical_profile,
               'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'functions':sorted(d.b.sizes(str(obj))),
               'verdicts':{n:d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n)) for n in d.b.sizes(str(obj)) if n in d.b.sizes(d.b.BLOB)},
               'changed_bodies':[n for n in d.b.sizes(str(obj)) if d.b.body(str(obj),n)!=d.b.body(str(baseline),n)],
               'metadata_equal_to_current':{key:a[key]==b[key] for key in a},'allocated_data':b['allocated'],'nontext_relocations':b['relocations']}
        assert entry['functions']==report['cells']['gentoo']['functions']
        matrix[label]=entry;(OUT/('retype-era-dumps.json' if args.dump_era else 'floor-stock.json' if args.floor_stock else 'retype-era.json' if args.retype_era else 'history.json')).write_text(json.dumps(matrix,indent=2)+'\n')
        print(label,'C2',entry['verdicts'][TARGET],'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),'/',len(entry['verdicts']))
if __name__=='__main__':main()
