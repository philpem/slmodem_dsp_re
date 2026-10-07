#!/usr/bin/env python3
"""Require dynamic forwarding witnesses and raw debugger equality, not empty logs."""
import argparse,hashlib,json,re
from pathlib import Path

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def trace_facts(text,cell,uid):
    assert 'exited normally' in text and 'Python Exception' not in text
    inputs=re.findall(r'TRACE UID%d simplify_set input (.*)'%uid,text)
    assert len(inputs)==2,(cell,inputs)
    wide=re.findall(r'TRACE UID38 simplify_set input (.*)',text)
    assert len(wide)==2 and all('ZERO_EXTEND:SImode(MEM:HImode' in x for x in wide)
    if cell=='baseline':
        assert all(x.startswith('SET:VOIDmode(MEM:HImode') for x in inputs)
        assert 'old_cost=4 new_cost=2' not in text
        return {'cell':cell,'selected_visits':2,'wide_visits':2,'forwarding':False}
    assert inputs[0].startswith('SET:VOIDmode(REG:HImode 0,MEM:HImode')
    assert inputs[1]=='SET:VOIDmode(REG:HImode 0,REG:HImode 0)'
    assert text.count('TRACE candidate REG:HImode 0 old_cost=4 new_cost=2')==1
    start=text.index('TRACE UID%d simplify_set input'%uid)
    end=text.index('TRACE record_set before UID%d'%uid,start)
    interval=text[start:end]
    assert 'TRACE return simplify_set = 1' in interval
    assert 'fallback operands' not in interval
    assert 'mode=HImode' in interval and "'REG:HImode 0'" in interval
    assert '[truncated]' not in text
    store,copy,increment=(47,54,53) if cell=='mask-postincrement' else (48,55,54)
    pattern=(r'TRACE record_set after UID%d value ptr=(0x[0-9a-f]+) value=(\d+) mode=HImode .* '
             r'addr ptr=(0x[0-9a-f]+) value=(\d+) mode=SImode')%store
    saved=re.search(pattern,text);assert saved,(cell,'store identity missing')
    hi_pointer,hi_value,addr_pointer,addr_value=saved.groups()
    copy_line=next(x for x in text.splitlines() if x.startswith('TRACE record_set after UID%d '%copy))
    assert 'ptr='+addr_pointer+' value='+addr_value+' mode=SImode' in copy_line
    increment_line=next(x for x in text.splitlines() if x.startswith('TRACE record_set after UID%d '%increment))
    assert 'ptr='+addr_pointer+' value='+addr_value+' mode=SImode' not in increment_line
    assert 'ptr='+addr_pointer+' value='+addr_value+' mode=SImode' in interval
    assert 'ptr='+hi_pointer+' value='+hi_value+' mode=HImode' in interval
    assert text.index(copy_line)<text.index(increment_line)<start
    return {'cell':cell,'selected_visits':2,'wide_visits':2,'forwarding':True,'rule':'simplify_set/cselib',
            'memory_cost':4,'register_cost':2,'stored_HI_value':hi_value,'old_address_value':addr_value,
            'old_address_survives_cursor_increment':True}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--artifacts',type=Path,required=True)
    ap.add_argument('--controls',type=Path,required=True);args=ap.parse_args()
    ledger=json.loads((args.artifacts/'results.json').read_text());rows=[]
    assert digest(args.artifacts/'cc1')==ledger['cc1_sha256']
    commands=json.loads((args.artifacts/'commands.json').read_text());assert commands and all(r['exit']==0 for r in commands)
    for control in ledger['controls']:
        cell=control['cell'];folder=args.artifacts/cell
        expected=digest(args.controls/'SDM'/cell/'candidate.o')
        assert digest(folder/'raw.o')==digest(folder/'traced.o')==expected
        assert set(control['object_hashes'].values())=={expected}
        assert digest(folder/'SDM.i')==control['input_sha256']==digest(args.controls/'SDM'/cell/'SDM.i')
        assert digest(Path(__file__).with_name('gentoo_cc1_sdm_trace.py'))==control['gdb_helper_sha256']
        text=(args.artifacts/(cell+'-trace.log')).read_text()
        rows.append(trace_facts(text,cell,control['selected_uid']))
    assert len(rows)==3 and sum(r['forwarding'] for r in rows)==2
    # A read-only detector must refuse a missing event, not silently certify it.
    positive=(args.artifacts/'mask-postincrement-trace.log').read_text()
    for missing in ('TRACE candidate REG:HImode 0 old_cost=4 new_cost=2','TRACE return simplify_set = 1'):
        try:trace_facts(positive.replace(missing,'removed-event'),'mask-postincrement',56)
        except AssertionError:pass
        else:raise AssertionError('missing dynamic witness accepted')
    report={'complete_object_comparisons':6,'selected_UID_visits':sum(r['selected_visits'] for r in rows),
            'wide_input_visits':sum(r['wide_visits'] for r in rows),'forwarding_positives':2,
            'baseline_negative':1,'missing_event_refusals':2,'source_or_RTL_writes':False,'rows':rows}
    (args.artifacts/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('3 controls / 6 raw-traced-saved object comparisons; 6 selected + 6 wide-input visits')
    print('2 simplify_set forwarding positives / 1 baseline negative / 2 missing-event refusals')

if __name__=='__main__':main()
