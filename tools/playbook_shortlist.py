#!/usr/bin/env python3
"""Screen small non-exact bodies and retain an auditable, curated 20-function shortlist."""
import argparse, json, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/toolchain'))
import byteident as b

SELECTED=('RenegotiateDetectV32','RetrainDetectV32','silence_is_more_then',
          'FIFO8_write','SetPulseBreakTime','SetPulseMakeTime','check_for_valid',
          '_ZN8FloatFIR15setCoefficientsEPfj','_ZN8FloatIIR15setCoefficientsEPfj',
          'RxHdxStartB103','cid_get_strings','GenerateCallingTone','ModDataV22',
          'TxHdxTRN','v23FP_tx_create','DetSequence','RxHdxSequenceE',
          'V22FP_modem','RxClampV22','TxNOP')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline-report',type=Path,required=True,help='Canonical byteident JSON, exact-name screening floor')
    ap.add_argument('--json-out',type=Path,required=True)
    args=ap.parse_args()
    exact=set(json.loads(args.baseline_report.read_text())['exact_symbols'])
    blob=b.sizes(b.BLOB);rows=[];shared=eligible=0
    objects=sorted((ROOT/'build/tc_out').glob('*.o'))
    # Prove that live-range classification fires in both directions.
    fir=str(ROOT/'build/tc_out/src_dsp_FloatFIR.cpp.o')
    fifo=str(ROOT/'build/tc_out/src_service_Fifo8.c.o')
    assert b.alpha_equal(b.insns(b.BLOB,'_ZN8FloatFIR5resetEv'),b.insns(fir,'_ZN8FloatFIR5resetEv'))
    assert not b.alpha_equal(b.insns(b.BLOB,'FIFO8_write'),b.insns(fifo,'FIFO8_write'))
    for p in objects:
        # Avoid active investigations; these are path exclusions, not service attribution.
        if any(x in p.name for x in ('_v34_','_v90_','_v92_','_v8_','_fax_','class1')):continue
        for n,size in b.sizes(str(p)).items():
            if n not in blob:continue
            shared+=1
            if n in exact or not 35<=blob[n]<=450 or abs(blob[n]-size)>32:continue
            eligible+=1
            v,d=b.verdict(*b.body(b.BLOB,n),*b.body(str(p),n))
            if v in ('EXACT','UNRESOLVED','NODATA'):continue
            rows.append(dict(symbol=n,object=str(p.relative_to(ROOT)),blob=blob[n],ours=size,verdict=v,detail=d))
    rows.sort(key=lambda r:(abs(r['ours']-r['blob']),r['blob'],r['symbol']))
    shortlist=[]
    for n in SELECTED:
        matches=[r for r in rows if r['symbol']==n]
        if not matches:continue
        r=matches[0].copy();r['regalloc_only']=b.alpha_equal(b.insns(b.BLOB,n),b.insns(str(ROOT/r['object']),n));shortlist.append(r)
    result=dict(objects=len(objects),shared_after_scope=shared,eligible=eligible,nonexact_rows=len(rows),
                baseline_report=str(args.baseline_report),known_regalloc_control=True,known_structural_control=True,
                rows=rows,shortlist=shortlist)
    args.json_out.parent.mkdir(parents=True,exist_ok=True)
    args.json_out.write_text(json.dumps(result,indent=2)+'\n')
    print('objects',len(objects),'shared after scope',shared,'eligible',eligible,'nonexact',len(rows),'curated shortlist',len(shortlist))
    print('known REGALLOC/structural controls: both fired')
    for r in shortlist:print(r['symbol'],r['verdict'],r['detail'],'register-only' if r['regalloc_only'] else 'structural')
if __name__=='__main__':main()
