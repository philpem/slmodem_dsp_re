#!/usr/bin/env python3
"""Replay issue252's bounded V34 switch and loop-boundary source families."""
import argparse
import hashlib
import json
import sys
import playbook_small_patterns as driver

FAMILY=None

def variants(path,source):
    global FAMILY
    if FAMILY=='combined':
        FAMILY='quick';quick=variants(path,source)['switch-result']
        FAMILY='snr-product';combined=variants(path,quick)['product-before-last']
        FAMILY='combined'
        return {'baseline':source,'combined':combined}
    name='VPcmV34GetQuickConnectIndication' if FAMILY=='quick' else 'VPcmV34GetSNR'
    start,end,fn=driver.function(source,name)
    forms={'baseline':fn}
    if FAMILY=='quick':
        head=fn[:fn.index('\n\tint mask;')]
        groups='''	case 0: case 1: case 2: case 5: case 6: case 7:
		ACTION_SHORT
	case 3: case 10:
		ACTION_ZERO
	case 4: case 8: case 9:
		ACTION_ONE
	default:
		ACTION_DEFAULT
'''
        returns=groups.replace('ACTION_SHORT','return obj->is_short;').replace('ACTION_ZERO','return 0;').replace('ACTION_ONE','return 1;').replace('ACTION_DEFAULT','return 0;')
        result=groups.replace('ACTION_SHORT','result = obj->is_short; break;').replace('ACTION_ZERO','result = 0; break;').replace('ACTION_ONE','result = 1; break;').replace('ACTION_DEFAULT','break;')
        forms['switch-return']=head+'\n\n\tswitch (obj->status) {\n'+returns+'\t}\n}'
        forms['switch-result']=head+'\n\tint result = 0;\n\n\tswitch (obj->status) {\n'+result+'\t}\n\treturn result;\n}'
    else:
        begin=fn.index('\tif (rx->equerr > 0) {')
        stop=fn.index('\n\tif (last > 0) {')
        first='''	int v = 0;

	if (rx->equerr > 0)
		v = rx->sig_energy / rx->equerr;

	while (v > 0) {
		last = v;
		v = (int)((unsigned int)v * 0x1013u) >> 14;
		if (v > 0)
			db += 6;
	}
'''
        flat=fn[:begin]+first+fn[stop:]
        forms['flat-first']=flat
        begin=flat.index('\tif (last > 0) {')
        stop=flat.index('\n\treturn db;')
        handoff='''	v = last;
	while (v > 0) {
		v = (int)((unsigned int)v * 0x32d6u) >> 14;
		if (v > 0)
			db += 1;
	}
'''
        forms['flat-handoff']=flat[:begin]+handoff+flat[stop:]
    if FAMILY=='snr-product':
        control=forms['flat-handoff']
        old='\t\tlast = v;\n\t\tv = (int)((unsigned int)v * 0x1013u) >> 14;'
        assert control.count(old)==1
        forms={'baseline':control,'product-before-last':control.replace(old,'\t\tint product = (int)((unsigned int)v * 0x1013u);\n\t\tlast = v;\n\t\tv = product >> 14;')}
    result={label:source[:start]+text+source[end:] for label,text in forms.items()}
    assert len(result)==len(set(result.values()))==(2 if FAMILY=='snr-product' else 3)
    return result

if __name__=='__main__':
    parser=argparse.ArgumentParser(add_help=False)
    parser.add_argument('--family',required=True,choices=('quick','snr','snr-product','combined'))
    options,remaining=parser.parse_known_args()
    FAMILY=options.family
    assert '--domain' in remaining and remaining[remaining.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/252')
    if FAMILY=='snr-product' and '--baseline-dir' not in remaining:
        prior=driver.ROOT/'build/gcc3-v34-accessor-snr'
        ledger=json.loads((prior/'results.json').read_text())
        entry=ledger['families']['VPcmV34Main']['cells']['flat-handoff']
        raw=(prior/'VPcmV34Main/flat-handoff/candidate.o').read_bytes()
        assert hashlib.sha256(raw).hexdigest()==entry['object_hash']
        control=driver.ROOT/'build/gcc3-snr-product-control'
        control.mkdir(exist_ok=True)
        (control/'.build-config').write_text(ledger['config'])
        (control/'src_pump_v34_VPcmV34Main.cpp.o').write_bytes(raw)
        remaining+=['--baseline-dir',str(control)]
    sys.argv=sys.argv[:1]+remaining
    driver.REV='3f8b8f77'
    driver.OUT_NAME='gcc3-v34-accessor-'+FAMILY
    driver.SOURCE_PATHS=('src/pump/v34/VPcmV34Main.cpp',)
    driver.variants=variants
    driver.main()
