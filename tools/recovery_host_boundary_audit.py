#!/usr/bin/env python3
"""Replay three bounded ordinary host-callback boundary controls."""
import json
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from recovery_literal_storage_audit import decode


def variants(path, source):
    return {'baseline':source}


def windows(path, name):
    with path.open('rb') as f:
        elf=ELFFile(f)
        sym=next(s for s in elf.get_section_by_name('.symtab').iter_symbols() if s.name==name)
        section=elf.get_section(sym['st_shndx']).name
        lo,size=sym['st_value'],sym['st_size']
    rows=[r for r in decode(path)[0] if r['section']==section and lo<=r['at']<lo+size]
    result=[]
    for i,row in enumerate(rows):
        targets=[t.split('-')[0] for at,kind,t in row['relocs'] if kind=='PC32']
        if any(t in ('modem_send_to_tty','modem_recv_from_tty','modem_set_param','modem_get_param') for t in targets):
            result.append({'call':row,'before':rows[max(0,i-16):i],'after':rows[i+1:i+13]})
    return result


if __name__=='__main__':
    d.REV='95422cd2'
    d.SOURCE_PATHS=('src/service/cid.c','src/v8/v8.c','src/service/voice.c')
    d.OUT_NAME='stage-c-host-boundary-control'
    d.DUMP_FLAGS=('-v','-save-temps')
    d.variants=variants
    d.main()
    root=d.ROOT/'build'/d.OUT_NAME
    raw=json.loads((root/'results.json').read_text())
    report={'source_changes':False,'families':{}}
    for family,name in (('cid','CID_process'),('v8','v8_process'),('voice','VOICE_process')):
        entry=raw['families'][family]['cells']['baseline']
        assert entry['baseline_reproduced']
        path=root/family/'baseline/candidate.o'
        report['families'][family]={'raw_full_TU_reproduced':True,'functions':len(entry['functions']),
            'exact_names':sorted(n for n,v in entry['verdicts'].items() if v[0]=='EXACT'),
            'original_host_boundaries':windows(Path(d.b.BLOB),name),
            'retained_host_boundaries':windows(path,name),'object_hash':entry['object_hash']}
    report['review']={
        'CID_process':'Original563/583 reload wrapper modem before each send, then strlen58b after both sends; source retains all three ages. Real Bell-202 success/checksum-failure histories already provide a valid component boundary. New read-only TTY wrappers snapshot rendered owner storage, argument provenance/length, modem identity and receiver fill at each actual call, forwarding unchanged data and return.',
        'v8_process':'Original3886/388c publish qc_lapm/qc_index before setparam9 at38da; reload dp.modem before getparam5 at38ee; expiry391d/392b publish idle_timer=-1/f1c=0 before setparam9=0 at3945. Retained source agrees. Existing planted wrapper statuses do not prove modem end-to-end reachability.',
        'VOICE_process':'Originalfd3 receive then state reloadfdd before possible second receiveff9; RXsend1054 precedes host_count publicatione41. Retained source agrees. Existing planted message scripts are component probes, not proof that each message history is reachable; no new mutating host/debug behavior assumed.',
        'limits':'No source correction identified in three leads. CID observers read only live constructor-owned objects and callback data; no private state is planted or callback mutation invented. Static windows establish operands/publication review, not arbitrary callback reentrancy safety.',
        'next_discriminator':'New independently mismatched argument/state publication at an ordinary host API, followed by a valid component history. Do not reopen these three value-age spellings or invent mutating diagnostic callbacks.'}
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('3 raw-identical full TU controls; host boundary windows retained; no source mismatch found')
