#!/usr/bin/env python3
"""Read existing EIA6 RTL and hash-pinned stock GCC sources; never compile."""
import argparse,hashlib,json,re,urllib.request
from pathlib import Path
import playbook_small_patterns
from gcc3_value_carriers_audit import function_chunk
from gcc3_reload_trace import expressions,instructions
ROOT=Path(__file__).resolve().parents[1]
HASHES={'reg-stack.c':'5af035ff9ef4070fdefb1d5ba787f09d16a8f3efff1c14dfc66e05fbccb120b0',
        'i386.c':'bb46f666e8686bf902ae1f9f32fcbfb298259915c1368e669326c89adce34cdc',
        'i386.md':'2b62f98bc15ccdc268036b57da4afe23f3f9e2c5fcfdda5175758e3dc62719ab'}

def nodes(path):
    text=function_chunk(path,'V90PreFilter::setParamEia6(')
    start=re.search(r'^\((?:insn|jump_insn|call_insn)(?::\S+)? ',text,re.M)
    assert start
    instructions(text)  # Refuse duplicate UIDs or truncated instruction streams.
    return [n for n in expressions(text[start.start():]) if n and str(n[0]).startswith(('insn','jump_insn','call_insn'))]

def deaths(node):
    if not isinstance(node,list):return []
    if node and node[0]=='expr_list:REG_DEAD':return [node[1]]+deaths(node[2])
    return sum([deaths(child) for child in node],[])

def record(node):
    return {'uid':int(node[1]),'pattern':next(child for child in node[2:] if isinstance(child,list)),
            'REG_DEAD':deaths(node)}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--gcc-source',type=Path,default=ROOT/'build/gcc-x87-mechanism-source')
    ap.add_argument('--dumps',type=Path,default=ROOT/'build/eia6-prior-controls/V90PreFilter')
    ap.add_argument('--output',type=Path,default=ROOT/'build/gcc-x87-mechanism.json')
    ap.add_argument('--fetch-source',action='store_true',help='Fetch the three hash-pinned official release files')
    args=ap.parse_args()
    if args.fetch_source:
        args.gcc_source.mkdir(parents=True,exist_ok=True)
        for name,expected in HASHES.items():
            suffix=name if name=='reg-stack.c' else 'config/i386/'+name
            url='https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-3.4.2/gcc/'+suffix
            with urllib.request.urlopen(url,timeout=30) as response:data=response.read()
            assert hashlib.sha256(data).hexdigest()==expected,('source hash mismatch',name)
            (args.gcc_source/name).write_bytes(data)
    for name,expected in HASHES.items():assert hashlib.sha256((args.gcc_source/name).read_bytes()).hexdigest()==expected,name
    report={}
    for label in ('baseline','rolled-unequal','expanded-relational','expanded-unequal'):
        cell={}
        for stage in ('33.sched2','34.stack'):
            stream=nodes(args.dumps/label/('V90PreFilter.cpp.'+stage))
            selected=[n for n in stream if any(op in str(record(n)['pattern']) for op in ('fix:SI','mult:XF','compare:CCFP'))]
            fixes=[n for n in selected if 'fix:SI' in str(record(n)['pattern'])]
            first=fixes[0];index=stream.index(first)
            cell[stage]={'operations':[record(n) for n in selected],
                         'first_conversion':record(first),'immediately_previous':record(stream[index-1])}
        before=cell['33.sched2']['first_conversion'];after=cell['34.stack']['first_conversion']
        assert before['uid']==after['uid'] and not before['REG_DEAD'] and after['REG_DEAD']
        assert 'reg/v:XF' in str(before['pattern']) and 'fix:SI' in str(after['pattern'])
        assert 'reg:XF' in str(cell['34.stack']['immediately_previous']['pattern'])
        report[label]=cell
    boundaries={}
    for stage in ('19.life','20.combine','22.regmove','24.lreg'):
        stream=nodes(args.dumps/'expanded-unequal'/('V90PreFilter.cpp.'+stage))
        boundaries[stage]={str(n[1]):record(n) for n in stream if str(n[1]) in ('181','232')}
    assert 'reg:SF' in str(boundaries['19.life']['232']['pattern'])
    assert 'mem/u/f:SF' in str(boundaries['20.combine']['232']['pattern'])
    assert 'mem/u/f:SF' not in str(boundaries['22.regmove']['181']['pattern'])
    assert 'mem/u/f:SF' in str(boundaries['24.lreg']['181']['pattern'])
    args.output.write_text(json.dumps({'stock_sources_sha256':HASHES,'cells':report,'operand_form_boundaries':boundaries,
                                      'stage_streams':12,'first_conversion_live_to_duplicate_pop_controls':4},indent=2)+'\n')
    print('4 existing cells / 12 stage streams: first live XF→SI conversion acquires duplicate and REG_DEAD at stack conversion')
    print('Register zero folds to memory at20.combine; register scale folds to memory by24.lreg')
    print('3 official stock source hashes verified; no compilation or source variants')
if __name__=='__main__':main()
