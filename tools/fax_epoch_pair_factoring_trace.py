#!/usr/bin/env python3
"""Audit the complete epoch TU and report observed RTL spill migration."""
import hashlib,json,re,subprocess
from pathlib import Path
import playbook_small_patterns as d
from gcc3_reload_trace import function,instructions,stack_home
from gcc3_value_carriers_audit import inspect

STAGES=['01.rtl','06.cse','08.gcse','20.combine','24.lreg','25.greg','33.sched2']
TARGET='V29RX_epoch_det'

def walk(node):
    if isinstance(node,list):
        yield node
        for child in node:yield from walk(child)

def stream(path):
    chunk=function(path.read_text(),TARGET)
    if path.name.endswith('.08.gcse'):
        # GCC prints intermediate and final streams; refuse duplicate UIDs,
        # select the explicitly labelled final basic-block stream first.
        assert chunk.count(';; Start of basic block 0,')==1
        chunk=chunk[chunk.index(';; Start of basic block 0,'):]
    return instructions(chunk)

def main():
    out=d.ROOT/'build/fax-epoch-pair-factoring'
    packages=['fax-epoch-pair-factoring','fax-epoch-index-boundary']
    rows=[]
    original_names=set(d.b.sizes(d.b.BLOB))
    original=d.b.body(d.b.BLOB,TARGET)
    original_dis=subprocess.check_output(['python3',str(d.ROOT/'tools/dis.py'),d.b.BLOB,TARGET],text=True,stderr=subprocess.DEVNULL)
    assert len(original[0])==413
    assert not re.search(r'\bcall\s',original_dis)
    assert len(original[1])==3
    for package in packages:
      family=d.ROOT/'build'/package/'V29rx'
      results=json.loads((family.parent/'results.json').read_text());cells=results['families']['V29rx']['cells']
      base=family/'baseline/candidate.o';base_meta=inspect(base)
      for label,cell in cells.items():
          obj=family/label/'candidate.o';source=family/label/'V29rx.c'
          assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash']
          assert hashlib.sha256(source.read_bytes()).hexdigest()==cell['source_hash']
          metadata=inspect(obj)
          for kind in ['records','allocated','nobits','relocations']:assert metadata[kind]==base_meta[kind],(label,kind)
          names=sorted(d.b.sizes(str(obj)));assert names==cell['functions']
          verdicts={n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n))) for n in names if n in original_names}
          assert verdicts==cell['verdicts']
          gains=[n for n in names if verdicts[n][0]=='EXACT' and cells['baseline']['verdicts'][n][0]!='EXACT']
          losses=[n for n in names if verdicts[n][0]!='EXACT' and cells['baseline']['verdicts'][n][0]=='EXACT']
          assert not gains and not losses
          changed=[n for n in names if d.b.body(str(obj),n)!=d.b.body(str(base),n)]
          assert set(changed)<={TARGET}
          if label=='baseline':assert cell['baseline_reproduced']
          stages=[]
          for suffix in STAGES:
              nodes=stream(family/label/('V29rx.c.'+suffix))
              operations={op:sum(1 for pattern in nodes.values() for n in walk(pattern) if n and n[0]==op) for op in ['mult:SI','ashiftrt:SI','sign_extend:SI']}
              homes=[{'uid':uid,'home':stack_home(n),'pattern':pattern} for uid,pattern in nodes.items() for n in walk(pattern) if stack_home(n) and stack_home(n)['offset']<=12]
              stages.append({'stage':suffix,'instructions':len(nodes),'operations':operations,'local_stack_accesses':homes,'pattern_hash':hashlib.sha256(json.dumps(list(nodes.values())).encode()).hexdigest()})
          rows.append({'package':package,'label':label,'emitted_bodies':len(names),'target_bytes':len(d.b.body(str(obj),TARGET)[0]),'verdict':verdicts[TARGET],'changed_bodies':changed,'gains':gains,'losses':losses,'stages':stages})
    baseline=next(r for r in rows if r['package']=='fax-epoch-pair-factoring' and r['label']=='baseline')
    streamed=next(r for r in rows if r['package']=='fax-epoch-pair-factoring' and r['label']=='streamed-pairs')
    for row in rows:
        assert all(s['operations']['mult:SI']==8 and s['operations']['ashiftrt:SI']==7 for s in row['stages'])
    # The known source-lifetime control changes spill roles before scheduling.
    bg=next(s for s in baseline['stages'] if s['stage']=='25.greg')['local_stack_accesses']
    sg=next(s for s in streamed['stages'] if s['stage']=='25.greg')['local_stack_accesses']
    assert any(n['uid']==51 and n['home']['offset']==4 and n['home']['mode']=='HI' and 'eq' in str(n['pattern']) for n in bg)
    assert not any('eq' in str(n['pattern']) for n in sg)
    assert any(n['uid']==173 and n['home']['offset']==0 and n['home']['mode']=='SI' for n in sg)
    assert baseline['target_bytes']==443 and streamed['target_bytes']==429
    repeated=d.ROOT/'build/fax-epoch-index-boundary/V29rx'
    assert (repeated/'baseline/candidate.o').read_bytes()==(out/'V29rx/baseline/candidate.o').read_bytes()
    assert (repeated/'streamed-control/candidate.o').read_bytes()==(out/'V29rx/streamed-pairs/candidate.o').read_bytes()
    summary={'repeated_raw_controls':2,'cells':len(rows),'function_comparisons':sum(r['emitted_bodies'] for r in rows),'stage_observations':len(rows)*len(STAGES),'original':{'bytes':413,'calls':0,'relocations':3},'firing_control':{'baseline_eq_hi_spill_uid':51,'streamed_distance_si_reload_uid':173},'rows':rows}
    (out/'trace-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('epoch factoring:',summary['cells'],'raw-valid TU cells,',summary['function_comparisons'],'live body comparisons,',summary['stage_observations'],'RTL observations;8 multiplies/7 SAR per stage; known eq→distance spill detector fired;0exactgains/losses,0bystander/metadata/data/BSS/nontext drift')
if __name__=='__main__':main()
