#!/usr/bin/env python3
"""Independent complete-path SAT witnesses and call-contract admission record."""
from pathlib import Path
import hashlib,json
import audit as a
p=Path(__file__).resolve().parent
out={'sides':{},'domain':{'object_config_session_unsigned_min':0x1000,'object_config_session_unsigned_max':0xd0000000,'entry_esp':0xf0000000,'debug_global':0xe0000000,'debug_string':0xe0000100,'mutual_owner_aliases_permitted':True,'owner_alias_with_debug_or_frame':False},'auditor_sha256':hashlib.sha256((p/'audit.py').read_bytes()).hexdigest()}
for name in ['native','candidate']:
    fun=a.elf_function(p/(name+'.o'));paths=a.execute(fun);rows=[]
    for state in paths:
        solver=a.z.Solver();solver.set(timeout=20000);solver.add(*a.DOMAIN,*state.constraints);result=solver.check();a.need(result==a.z.sat,'complete path lacks independent SAT witness')
        model=solver.model();role=next(s['_loads'][0][1] for s in state.trace if s['offset']==0xfb)
        row={'signature':a.signature(state),'result':str(result),'conditions':[c.sexpr() for c in state.constraints],'witness':{k:model.eval(v,model_completion=True).as_long() for k,v in {'object':a.O,'config':a.C,'session':a.SESSION,'message':a.MSG,'late':a.LATE,'flag':a.FLAG,'debug_word':a.DEBUG,'post_tone_role_low_word':role}.items()}}
        rows.append(row)
    out['sides'][name]={'sha256':fun['sha256'],'feasible_complete_paths':len(rows),'sat':sum(r['result']=='sat' for r in rows),'unknown':0,'paths':rows}
x=json.loads((p/'audit.json').read_text());contracts=[]
for path in x['paths']:
    admitted=True
    for event in path['observations']:
        if event['kind']=='call':
            admitted=admitted and event['all_equal'];contracts.append({'path':path['id'],'callee':event['target'],'pre_call_state_and_arguments_equal':event['all_equal'],'all_prior_call_preconditions_discharged':admitted,'post_call_shared_symbols_admitted':admitted})
out['relational_call_contracts']={'denominator':len(contracts),'admitted':sum(c['post_call_shared_symbols_admitted'] for c in contracts),'rows':contracts}
(p/'path-feasibility.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'complete_path_SAT_checks':sum(v['feasible_complete_paths'] for v in out['sides'].values()),'SAT':sum(v['sat'] for v in out['sides'].values()),'UNKNOWN':0,'call_contracts':out['relational_call_contracts']['denominator'],'admitted':out['relational_call_contracts']['admitted']}))
