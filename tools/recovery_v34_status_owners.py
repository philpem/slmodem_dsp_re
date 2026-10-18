#!/usr/bin/env python3
"""Bounded native V34 status output census and manually reviewed owner ledger.

No compiler execution, source edits, reachability claim or equivalence proof.
X87 FST operand access metadata needs explicit treatment: the known-positive
float fields0x70/0x74 calibrate that path. Omitting one output must fail.
"""
import argparse,copy,hashlib,json,re,subprocess
from pathlib import Path
import recovery_float_decision_screen as s
import playbook_small_patterns as d
from capstone import CS_AC_WRITE
from capstone.x86 import X86_OP_MEM

ROOT=s.ROOT
FIELDS={0x6c:('float_06c','V34/V90: -12.0f minus obj signed16+25dc tx_pwr_reduction; V92: literal-12'),0x70:('float_070','float VPcmV34GetSNR(obj), native inlined receiver+21a signed16 equerr and receiver+248 signed32 energy; receiver=obj+264'),0x74:('float_074','same SNR value as+70'),0x80:('word_080','obj signed16+aa7e rtd, sign extended then unsigned cast'),0x84:('roundTripDelay','same unscaled rtd as+80'),0xb0:('txBaudRate','obj signed16+aa84 ratecfg.baud; V92analog literal8000'),0xb4:('rxBaudRate','obj signed16+aa96 ratecfg.rx_baud'),0xb8:('txCarrier','obj signed16+aa94 ratecfg.carrier; V92analog literal0'),0xbc:('rxCarrier','obj signed16+aaa8 ratecfg.rx_carrier'),0xc0:('txBpsLatched','obj signed32+ac04 tx_bps'),0xc4:('rxBpsLatched','obj signed32+ac08 rx_bps'),0xe8:('word_0e8','obj signed16+ac12 short_ac12'),0xec:('word_0ec','sum obj signed16+ac0e rrn_local and+ac10 rrn_remote'),0xf0:('word_0f0','same short_ac12 as+e8'),0xf4:('word_0f4','obj signed16+ac14 short_ac14'),0xf8:('txDataRate','V34/V90:2400*obj signed16+aa88 ratecfg.txbits; V92analog:obj+3548->session+6124->modulator+4c->bitsToSymbol+0->transmitter+4 unsignedK, unsigned8000*K then float1/12 and+.5'),0xfc:('dataRate','2400*obj signed16+aa98 ratecfg.rxbits'),0x100:('word_100','same short_ac12 as+e8'),0x228:('word_228','obj signed16+ac0c v90_timing_offset; zero maps to-1')}
TARGET='VPcmV34GetDiagnostics'

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def stores(rows,owner):
    result={}
    for i in rows:
        if not i.operands:continue
        op=i.operands[0]
        if op.type!=X86_OP_MEM or i.reg_name(op.mem.base)!=owner:continue
        if not (op.access&CS_AC_WRITE or i.mnemonic in ('fst','fstp')):continue
        assert not op.mem.index
        result.setdefault(op.mem.disp,[]).append({'address':hex(i.address),'width':op.size,'bytes':i.bytes.hex(),'mnemonic':i.mnemonic,'operands':i.op_str})
    return result

def validate(output):
    assert set(output)==set(FIELDS),('output destinations',sorted(output))
    assert all(row['width']==4 for rows in output.values() for row in rows)
    assert any(row['mnemonic']=='fst' for row in output[0x74])
    assert any(row['mnemonic']=='fstp' for row in output[0x70])

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,default=ROOT/'build/v34-status-owners293');args=ap.parse_args();args.out.mkdir(exist_ok=True)
    blob=Path(d.b.BLOB).resolve();native=s.load(blob);ours=ROOT/'build/period/src_pump_v34_v34diag.o';current=s.load(ours)
    outputs={}
    for label,rows,owner in [('native',native[TARGET],'esi'),('current',current[TARGET],'ebx')]:
        output=stores(rows,owner);validate(output);outputs[label]={hex(k):v for k,v in output.items()}
    positive=stores(native[TARGET],'esi');negative=copy.deepcopy(positive);negative.pop(0x74)
    try:validate(negative)
    except AssertionError:pass
    else:raise AssertionError('omitted float store accepted')
    source=ROOT/'src/pump/v34/v34diag.cpp';text=source.read_text();start=text.index('\n'+TARGET+'(');body=text[start:text.index('\n}\n',start)+3]
    assert set(re.findall(r'results->(\w+)',body))=={v[0] for v in FIELDS.values()}
    # Source expression observations, checked independently of prose comments.
    for expr in ('cfg->baud','cfg->rx_baud','cfg->carrier','cfg->rx_carrier','cfg->txbits','cfg->rxbits','obj->tx_bps','obj->rx_bps','obj->rrn_local + obj->rrn_remote','obj->short_ac14','obj->short_ac12','obj->v90_timing_offset','obj->rtd','mod->bitsToSymbol->transmitter->K'):
        assert expr in body,expr
    obj=ROOT/'build/period/src_pump_v34_VPcmV34Main.o';getters={}
    for name in d.b.sizes(str(obj)):
        if name.startswith(('VPcmV34Get','GetVPcm','getTiming')):
            getters[name]={'verdict':list(d.b.verdict(*d.b.body(str(blob),name),*d.b.body(str(obj),name))),'native':d.b.insns(str(blob),name),'current':d.b.insns(str(obj),name)}
    assert len(getters)==14 and sum(v['verdict'][0]=='EXACT' for v in getters.values())==11
    for name in (TARGET,'GetVPcmMinimalTxPowerReduction','VPcmV34GetMaxUpstreamRateIndex','VPcmV34GetCurrentTxBitRate'):
        (args.out/(name+'.native.dis')).write_text(subprocess.check_output(['python3',str(ROOT/'tools/dis.py'),str(blob),name],text=True,stderr=subprocess.DEVNULL))
    report={'revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'input_hashes':{str(p):sha(p) for p in (blob,ours,obj,source,ROOT/'src/pump/v34/VPcmV34Main.cpp',ROOT/'include/dsplib/v34fsk.h',ROOT/'include/dsplib/v34recv.h',ROOT/'include/dsplib/TAG_DiagnosticResults.h',ROOT/'include/dsplib/VPcmFloModem.h')},'diagnostic_verdict':list(d.b.verdict(*d.b.body(str(blob),TARGET),*d.b.body(str(ours),TARGET))),'output_fields':{hex(k):{'field':v[0],'manual_native_to_source_review':v[1]} for k,v in FIELDS.items()},'output_store_sites':outputs,'output_field_count':19,'native_instruction_count':len(native[TARGET]),'current_instruction_count':len(current[TARGET]),'controls':'All19 fields found in both bodies; known x87 positive0x70/0x74; dropped0x74 rejected.14publicgetters/11 exact controls.','getters':getters,'manual_nonexact_getter_review':{'GetVPcmMinimalTxPowerReduction':'obj+ac3c->config+44 signed16 clamp[-10,7],config+54 codec;obj+3548->session+6120 info0Layout and+610c params;+4f8 sensitive;+4f4 highleveltx writes;obj+24c/+250 receiver flags;obj+3554/+3558/+355c echo publications. All source fields agree. Native7b67 rereads params pointer at session+610c; current also rereads it. Existing comment incorrectly calls that a reloaded session pointer.','VPcmV34GetMaxUpstreamRateIndex':'obj+ac3c->config+3c maxRate;obj+3548->session+6120/+610c,params+4f8 sensitive and+4fc cap. All agree;no offset bug.','VPcmV34GetCurrentTxBitRate':'obj+3548->session;role signed16+359c,status32+0;fallback signed16+aa88;V90 session+1758->tx+2c state,+40 bitsToSymbol->+0 mapper->+4 bitsPerFrame;V92 session+6124->tx+2c phase,+4c bitsToSymbol->+0 transmitter->+4 K. All agree.'},'limits':'Result registers are manually verified fixed callee-saved owners (nativeESI/currentEBX); this census covers stores addressed through those owners and checks source assignment destination names, not arbitrary alias stores or delegated getAT_UD output fields. Native source-owner ledger is manual instruction-backed review, not inferred automatically from equal destination sets. Current SIZE40 diagnostic mismatch includes native inlineSNR versus source out-of-line call; same destinations do not prove path equivalence. Existing t_v34diag planted-object fixtures are exploratory component probes, not lifecycle reachability proof. No behavior discrepancy demonstrated; no new source or test mutation licensed.'}
    (args.out/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('19/19 output fields in both bodies;1 float-store omission refused;14getter bodies reviewed/11 EXACT;0 wrong-owner discrepancies demonstrated')

if __name__=='__main__':main()
