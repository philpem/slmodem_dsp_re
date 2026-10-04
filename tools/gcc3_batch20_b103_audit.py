#!/usr/bin/env python3
"""Audit bounded B103 missing-call/source controls, all complete period TUs."""
import json,re
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
text=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(text[text.index('def inspect'):text.index('reports=')])
allowed_imports={'dsplibs_debug_level','dsplibs_debug_printf'}
strings={'b103','Bell103','V.21','b103: create...\n','b103: %s config %d,%d,%d,%d,%d %d','b103: B103 state -> %x (msg: %x)\n','b103: ReturnStatus = BELL_103_LINKED\n'}
reports={};total=0
for suffix in ('','-next','-order','-types','-owner','-parameter','-result','-buffers'):
    root=driver.ROOT/'build'/('gcc3-batch20-v90-b103'+suffix)
    cells=json.loads((root/'results.json').read_text())['families']['b103']['cells']
    base=inspect(root/'b103/baseline/candidate.o')
    for label,cell in cells.items():
        path=root/'b103'/label/'candidate.o';report=inspect(path)
        assert len(b.sizes(str(path)))==5
        assert report['objects']==base['objects'],(suffix,label,'named data')
        added=set(report['records'])-set(base['records'])
        assert added<=allowed_imports,(suffix,label,added)
        assert set(base['records'])<=set(report['records'])
        for name,rec in base['records'].items():assert report['records'][name]==rec,(suffix,label,name)
        for name in added:assert report['records'][name]==['STT_NOTYPE','STB_GLOBAL','STV_DEFAULT','SHN_UNDEF',0]
        with path.open('rb') as stream:
            elf=ELFFile(stream)
            allocated=report['allocated_sizes']
            assert set(allocated)<={'.data','.bss','.rodata.str1.1','.rodata.str1.4'},(suffix,label,allocated)
            assert allocated.get('.data')==24 and allocated.get('.bss')==0,(suffix,label,allocated)
            assert sum(len(bytes.fromhex(v['bytes'])) for v in report['objects'].values() if v['section']=='.data')==24
            for sec in elf.iter_sections():
                if isinstance(sec,RelocationSection):
                    target=elf.get_section(sec['sh_info'])
                    assert target.name in ('.text','.data'),(suffix,label,sec.name,target.name)
            found={v.decode() for sec in elf.iter_sections() if sec.name.startswith('.rodata.str') for v in sec.data().split(b'\0') if v}
            assert found<=strings,(suffix,label,found-strings)
        assert not cell.get('losses')
        assert set(cell.get('changed_bodies',[]))<={'b103_create','b103_process','dp_b103_exit','dp_b103_init','b103_delete'}
        reports[suffix+'/'+label]={'gains':cell.get('gains',[]),'changed_bodies':cell.get('changed_bodies',[]),'imports_added':sorted(added),'strings':sorted(found)}
        total+=1
assert total==71,total
selected=[('','baseline'),('','debug-id-branch-predecrement-status-switch'),('-order','combined-object-order'),('-types','zero-name'),('-owner','self-rate-owner-byte-print')]
records={}
for suffix,label in selected:
    folder=driver.ROOT/'build'/('gcc3-batch20-v90-b103'+suffix)/'b103'/label
    for stage in ('01.rtl','20.combine','25.greg','28.peephole2','31.bbro','33.sched2'):
        text=(folder/('b103.c.'+stage)).read_text()
        for fn in ('b103_create','b103_process','dp_b103_exit'):
            bodies=[s for s in re.split(r'^;; Function ',text,flags=re.M)[1:] if s.splitlines()[0].strip()==fn]
            assert len(bodies)==1,(label,stage,fn)
            patterns=instructions(bodies[0]);records[suffix+'/'+label+'/'+stage+'/'+fn]=patterns
            if stage=='01.rtl' and fn in ('b103_create','b103_process'):
                calls=[p for p in patterns.values() if 'dsplibs_debug_printf' in str(p) and 'call' in str(p)]
                assert len(calls)==(0 if label=='baseline' else 2),(label,fn,len(calls))
winner=driver.ROOT/'build/gcc3-batch20-v90-b103-owner/b103/self-rate-owner-byte-print/candidate.o'
for fn in ('b103_create','dp_b103_exit'):
    assert b.verdict(*b.body(b.BLOB,fn),*b.body(str(winner),fn))==('EXACT',0)
(driver.ROOT/'build/batch20-b103-full-tu-audit.json').write_text(json.dumps({'valid_cells':total,'functions_per_cell':5,'reports':reports,'stage_records':len(records),'initial_debug_call_controls':10},indent=2)+'\n')
(driver.ROOT/'build/batch20-b103-stage-patterns.json').write_text(json.dumps(records,indent=2)+'\n')
print('71/71 complete TUs: 5 functions; canonical ops table/metadata preserved; only two proved debug imports and seven authentic strings')
print('90/90 stage records; 10/10 missing-debug controls; create406B/exit49B EXACT, no losses; process remainsSIZE11')
