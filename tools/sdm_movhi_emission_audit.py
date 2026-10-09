#!/usr/bin/env python3
"""Prove MOVZWL can emit plain HI RTL; reject source widening from opcode alone."""
import hashlib,json,re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_reload_trace import function,instructions


def stream(path):
    text=function(path.read_text(),'SDM_descrambler')
    if path.name.endswith('25.greg'):
        marker=';; Start of basic block 0,';assert text.count(marker)==1;text=text[text.index(marker):]
    return instructions(text)


def emitted(assembly,uid):
    start=assembly.index('SDM_descrambler:\n')
    end=assembly.index('\t.size\tSDM_descrambler',start)
    assembly=assembly[start:end]
    # Select by the assembler's UID annotation; inspect adjacent complete RTL.
    lines=assembly.splitlines()
    indexes=[i for i,line in enumerate(lines) if re.search(r'# '+str(uid)+r'\s',line) and not line.startswith('#')]
    assert len(indexes)==1,(uid,indexes)
    index=indexes[0];start=max(i for i in range(index) if lines[i].startswith('#(insn'))
    return '\n'.join(lines[start:index+1])


def main():
    root=d.ROOT/'build/sdm-postreload';ledger=json.loads((root/'results.json').read_text())
    plain=root/'SDM/mask-postincrement';compound=root/'SDM/compound-postincrement'
    counts=[]
    for stage in ('01.rtl','06.cse','19.life','20.combine','25.greg'):
        pattern=stream(plain/('SDM.c.'+stage))[56]
        assert pattern[0]=='set' and pattern[1][0]=='reg:HI' and pattern[2][0]=='mem:HI'
        counts.append({'stage':stage,'plain_UID56':pattern})
    folder=compound
    cell=ledger['families']['SDM']['cells']['compound-postincrement']
    assert hashlib.sha256((folder/'candidate.o').read_bytes()).hexdigest()==cell['object_hash']
    pattern=stream(folder/'SDM.c.35.mach')[118]
    assert pattern[1][0]=='reg:HI' and pattern[2][0]=='mem:HI'
    assembly=folder/'SDM.s'
    replay=d.ROOT/'build/gentoo-cc1-trace-full/compound-postincrement/raw.s'
    assert assembly.read_bytes()==replay.read_bytes(), 'annotated assembly replay drift'
    text=emitted(assembly.read_text(),118)
    assert '{*movhi_1}' in text and '*movhi_1/3' in text
    assert re.search(r'movzwl\s+\(%edi\), %edx',text)
    assert 'zero_extend:' not in text
    wide=emitted((folder/'SDM.s').read_text(),38)
    assert 'zero_extend:SI' in wide and '*zero_extendhisi2_movzwl' in wide
    assert 'movzwl' in wide
    # Same observed opcode, different proven RTL modes; never collapse their provenance.
    result={'plain_HI_read_stage_checks':len(counts),'assembly_patterns':2,
            'plain_HI_emitter':'movhi_1/3','explicit_SI_emitter':'zero_extendhisi2_movzwl',
            'observed_mnemonic':'movzwl','original_RTL_mode_from_mnemonic':'underdetermined',
            'plain_read_stages':counts,'plain_HI_assembly':text,'explicit_SI_assembly':wide}
    (root/'movhi-emission-audit.json').write_text(json.dumps(result,indent=2)+'\n')
    print('5 saved stage checks: late plain reread already HI in initial RTL, no combine narrowing')
    print('2 annotated MOVZWL patterns: plain movhi_1 and explicit SI zero_extend; original mode underdetermined')

if __name__=='__main__':main()
