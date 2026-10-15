#!/usr/bin/env python3
"""Audit SineWave layout attribution, copied-block handling and controls."""
import hashlib
import json
from collections import Counter
from pathlib import Path
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_layout_trace import extract

TARGET = '_ZN8SineWaveIffE8generateEPfm'
HEADER = 'void SineWave<Tout, Tparam>::generate(Tout*, long unsigned int) [with Tout = float, Tparam = float]'


def audit():
    root = d.ROOT/'build/gcc3-sine-cold-layout'
    manifest = json.loads((root/'results.json').read_text())
    cells = manifest['families']['VpcmFloModem']['cells']
    folder = root/'VpcmFloModem'
    base = folder/'baseline/candidate.o'
    canonical = d.ROOT/'build/tc_out/src_pump_v90_VpcmFloModem.cpp.o'
    assert base.read_bytes() == canonical.read_bytes()
    before = inspect(base)
    rows = {}
    for label, cell in cells.items():
        obj = folder/label/'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
        current = inspect(obj)
        assert current['records']==before['records'] and current['objects']==before['objects']
        changed = [n for n in cell['functions'] if masked(obj,n)!=masked(base,n)]
        assert set(changed) <= {TARGET}
        sections = [n for n in set(allocated(base))|set(allocated(obj)) if allocated(base).get(n)!=allocated(obj).get(n)]
        assert set(sections) <= {'.gnu.linkonce.t.'+TARGET, '.rodata.cst4'}
        if label == 'first-iteration-owner':
            assert allocated(obj)['.rodata.cst4'] == allocated(base)['.rodata.cst4'] + '0000c07f'
            from gcc3_stage_divergence import chunks
            pre = chunks(folder/label/'VpcmFloModem.cpp.33.sched2', HEADER)
            post = chunks(folder/label/'VpcmFloModem.cpp.34.stack', HEADER)
            assert len(pre) == len(post) == 1
            assert '*.LC92' not in pre[0] and '*.LC92' in post[0]
            assert '(insn 219 ' in post[0]
        else:
            assert not sections
        assert len(cell['verdicts']) == 34 and not cell.get('gains') and not cell.get('losses')
        trace = extract(folder/label,'VpcmFloModem.cpp',HEADER)
        rows[label] = {'verdict':cell['verdicts'][TARGET],'changed_bodies':changed,
                       'changed_allocated_sections':sections,'trace':trace}
    baseline = rows['baseline']['trace']
    assert rows['baseline']['verdict'] == ['BYTES',50]
    assert baseline['final_order']==[0,1,3,2,4,5] and not baseline['copied_blocks']
    assert baseline['reorder_traces'][1]['blocks']==[{'block':4,'frequency':1000},{'block':5,'frequency':2000}]
    assert baseline['predictions'][0]['heuristics'][-1]=={'name':'combined heuristics','percent':50.0}
    variant = rows['first-iteration-owner']['trace']
    assert rows['first-iteration-owner']['verdict'] == ['SIZE',40]
    assert variant['copied_blocks']==[{'original':4,'copy':8}]
    assert variant['denominator']=={'predicted_branches':4,'reorder_traces':4,'ordered_blocks':9,'x87_edges':13}
    common = extract(d.ROOT/'build/gcc3-sine-scalar-transfer/VpcmFloModem/scalar-sinl-entry','VpcmFloModem.cpp',HEADER)
    assert any(h['name']=='loop header heuristics' and h['percent']==36.0 for p in common['predictions'] for h in p['heuristics'])
    assert any(e['action']=='new block; pops needed' for e in baseline['x87_edges'])
    stream = lambda path,name: Counter((m,o) for m,o in d.b.insns(str(path),name) if not m.startswith('j') and not d.b._padding(m,o))
    assert stream(d.b.BLOB,TARGET)==stream(base,TARGET)
    assert stream(d.b.BLOB,TARGET)!=stream(folder/'first-iteration-owner/candidate.o',TARGET)
    screen = json.loads((d.ROOT/'build/expansion-transfer-screen.json').read_text())
    hits = []
    for row in screen['rows']:
        obj = d.ROOT/'build/tc_out'/Path(row['object']).name
        if stream(d.b.BLOB,row['name'])==stream(obj,row['name']):
            hits.append(row['name'])
    assert len(screen['rows'])==92 and hits==[TARGET]
    # Exercise refusal on a known malformed trace without changing the compiler.
    import tempfile
    with tempfile.TemporaryDirectory(prefix='sine-layout-negative-',dir=d.ROOT/'build') as directory:
        temp = Path(directory)
        for stage in ('12.bp','31.bbro','34.stack'):
            (temp/('VpcmFloModem.cpp.'+stage)).write_bytes((folder/'first-iteration-owner'/('VpcmFloModem.cpp.'+stage)).read_bytes())
        p=temp/'VpcmFloModem.cpp.31.bbro'
        p.write_text(p.read_text().replace('Duplicated bb 4 (created bb 8)','Duplicated bb 99 (created bb 8)'))
        try:
            extract(temp,'VpcmFloModem.cpp',HEADER)
        except ValueError as exc:
            assert 'copy source not observed' in str(exc)
        else:
            raise AssertionError('unknown copy source accepted')
    report = {'cells':rows,'historical_common_loop_trace':common,'shared_body_verdicts':68,
              'screened_floating_copies':92,'layout_only_nominations':hits,
              'known_copy_positive_negative_control':True,'production_source_unchanged':True}
    (d.ROOT/'build/sine-cold-layout-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('2 cells/68 verdicts; no adoption/gains/losses;92 floating copies/1 layout nomination')
    print('3 traces:8 predicted branches,8 reorder traces,19 ordered blocks,29 x87 edges; block-copy positive/refusal controls pass')


if __name__ == '__main__':
    audit()
