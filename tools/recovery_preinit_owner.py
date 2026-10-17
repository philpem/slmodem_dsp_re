#!/usr/bin/env python3
"""Test natural preinitV34 visibility in the complete shell/digital owner pair.

The original embeds two preinitialisation bodies, while the current separate
TU calls preinitV34 twice. Historical merge 7fe8c8b24 introduced that boundary
when reconciling duplicate reconstructions (F188). Original TU attribution is
unsettled. Test one same-owner source cell after initV34, its preceding original
symbol, with ordinary retained O3 flags. No inline attributes or profile knobs.
"""
import argparse,hashlib,json,re,shlex,subprocess,sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_fax_debug_audit import Object
from recovery_fixedrc_apparatus import allocated
from recovery_generator_separation import named_data


def sources(late=False):
    digital=subprocess.check_output(['git','show','06ba1f713:src/pump/v34/v34digital.c'],cwd=d.ROOT,text=True)
    shell=subprocess.check_output(['git','show','06ba1f713:src/pump/v34/v34shell.c'],cwd=d.ROOT,text=True)
    a,z,fn=d.function(digital,'preinitdigital')
    start=digital.rfind('\nvoid\n',0,a)+1
    assert start>0
    comment_start=digital.rfind('/*\n * Bring the digital half up.',0,start)
    assert comment_start>0
    body=digital[comment_start:z]
    comment='''\t/*
\t * `preinitV34` is the object's own, exported and reconstructed in
\t * v34shell.c.  It takes the shell's FIELDS pointer -- 0xa00 into the
\t * context -- which is how every call site in the object spells it.
\t */'''
    assert body.count(comment)==1
    body=body.replace(comment,'''\t/* The original embeds both field initialisers here.  Keeping the
\t * body visible lets the period compiler inline its ordinary calls. */''')
    if late:
        body,count=re.subn(r'\tstruct v34_shell \*tx = .*?;\n','',body,count=1,flags=re.S)
        assert count==1
        body=body.replace('preinitV34((char *)tx + V34_SHELL_FIELDS);',
                          'preinitV34((char *)obj + V34_SHELL_TX + V34_SHELL_FIELDS);')
        body=body.replace('\tif (obj->role == 0x65) {',
            '\t{\n\tstruct v34_shell *tx = (struct v34_shell *)((char *)obj + V34_SHELL_TX);\n\n\tif (obj->role == 0x65) {')
        body=body[:-1]+'\t}\n}'
    _,at,_=d.function(shell,'initV34')
    assert 'preinitdigital(void *objp)' not in shell
    return {'v34digital':digital[:comment_start]+digital[z:],
            'v34shell':shell[:at]+'\n\n'+body+shell[at:]}


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--late-role-owner',action='store_true');ap.add_argument('--formatted-production',action='store_true');ap.add_argument('--baseline-dir',type=Path,help='prepare raw baseline pair from this canonical object directory first');args=ap.parse_args()
    label='formatted-production' if args.formatted_production else 'late-role-owner' if args.late_role_owner else 'same-owner'
    root=d.ROOT/'build/preinit-owner'
    if args.baseline_dir:
        saved_argv=sys.argv[:]
        sys.argv=[sys.argv[0],'--domain',str(Path(__file__).resolve()),'--baseline-dir',str(args.baseline_dir)]
        d.REV='06ba1f713';d.OUT_NAME='preinit-owner'
        d.SOURCE_PATHS=('src/pump/v34/v34digital.c','src/pump/v34/v34shell.c')
        d.DUMP_FLAGS=('-da','-v','-save-temps');d.variants=lambda path,source:{'baseline':source}
        try:d.main()
        finally:sys.argv=saved_argv
    manifest=json.loads((root/'results.json').read_text())
    result={'revision':manifest['revision'],'config':manifest['config'],'cells':{}}
    for family,text in sources(args.late_role_owner).items():
        if args.formatted_production:text=(d.ROOT/'src/pump/v34'/(family+'.c')).read_text()
        base=root/family/'baseline';out=root/family/label;out.mkdir(exist_ok=True)
        (out/(family+'.c')).write_text(text)
        cell=manifest['families'][family]['cells']['baseline']
        assert cell['baseline_reproduced']
        command=[s.replace('/'+family+'/baseline','/'+family+'/'+label) for s in cell['command']]
        with (out/'compile.log').open('w') as log:
            code=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
        assert code==0,(family,code)
        obj=out/'candidate.o'
        if args.formatted_production:
            assert obj.read_bytes()==(root/family/'late-role-owner/candidate.o').read_bytes(), 'formatting moved code'
        result['cells'][family]={'command':command,'compile_exit':code,'source_hash':hashlib.sha256(text.encode()).hexdigest(),
          'object_hash':hashlib.sha256(obj.read_bytes()).hexdigest()}
    baseline={};candidate={};old_globals=[];new_globals=[];old_defs=[];new_defs=[];old_imports=set();new_imports=set();baseline_exact=[];candidate_exact=[]
    for family in sources():
        a=root/family/'baseline/candidate.o';b=root/family/label/'candidate.o'
        x=Object(a).canonical();y=Object(b).canonical()
        assert x[1]==y[1],(family,'nontext data')
        assert named_data(a)==named_data(b)
        assert {n:v for n,v in allocated(a).items() if n!='.text'}=={n:v for n,v in allocated(b).items() if n!='.text'}
        old_globals+=x[2];new_globals+=y[2]
        for path,defs,imports,exact in ((a,old_defs,old_imports,baseline_exact),(b,new_defs,new_imports,candidate_exact)):
            obj=Object(path)
            for symbol in obj.symbols:
                if symbol['st_info']['bind']=='STB_LOCAL':continue
                if symbol['st_shndx']=='SHN_UNDEF':imports.add(symbol.name)
                else:defs.append((symbol.name,symbol['st_info']['bind'],symbol['st_info']['type'],symbol['st_other']['visibility']))
            for name in obj.canonical()[0]:
                if name in d.b.sizes(d.b.BLOB) and d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(str(path),name))[0]=='EXACT':exact.append(name)
        baseline.update(x[0]);candidate.update(y[0])
        for n in set(x[0])&set(y[0]):
            assert x[0][n]==y[0][n],(family,n,'bystander body drift')
            assert x[3].get(n)==y[3].get(n),(family,n,'bystander consumers')
    assert set(baseline)==set(candidate)
    # Pair definitions retain binding/visibility. preinitV34's old cross-TU
    # import is resolved within the new owner; external pair imports agree.
    assert sorted(old_defs)==sorted(new_defs),'defined binding or visibility drift'
    assert sorted(baseline_exact)==sorted(candidate_exact),'exact-set drift'
    external=lambda imported,defined:sorted(imported-{r[0] for r in defined})
    assert external(old_imports,old_defs)==external(new_imports,new_defs),'pair external-import drift'
    result['defined_global_count']=len(old_defs)
    result['defined_globals']=sorted(old_defs)
    result['external_imports']=external(old_imports,old_defs)
    result['exact_before']=sorted(baseline_exact);result['exact_after']=sorted(candidate_exact)
    for family in sources():
        a=root/family/'baseline/candidate.o';b=root/family/label/'candidate.o'
        result['cells'][family]['globals_before']=subprocess.check_output(['nm','-g',str(a)],text=True)
        result['cells'][family]['globals_after']=subprocess.check_output(['nm','-g',str(b)],text=True)
    original=d.b.body(d.b.BLOB,'preinitdigital')
    new=d.b.body(str(root/'v34shell'/label/'candidate.o'),'preinitdigital')
    result['verdict']=d.b.verdict(*original,*new)
    result['pair_bodies']=len(baseline)
    result['source_mapping']='Keep v34digital.c in normal build as layout-only TU; preinitdigital moves to v34shell.c. No header/type definitions change.'
    result['next_discriminator']='Original publishes scrambler/descrambler state through group-base LEAs and reversed word clears; candidate uses direct object-field ascending stores. Trace that independent owner/publication graph before register-only changes.'
    result['changed_bodies']=[n for n in baseline if baseline[n]!=candidate[n]]
    result['limits']='Source placement is a visibility hypothesis supported by the two original embedded bodies and prior functional deduplication; exact original TU extent remains unestablished.'
    (root/(label+'-audit.json')).write_text(json.dumps(result,indent=2)+'\n')
    print(label+':',result['verdict'],';',len(baseline),'pair bodies;',result['changed_bodies'])


if __name__=='__main__':main()
