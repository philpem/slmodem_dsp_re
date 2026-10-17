"""ELF-backed positive/rejection controls; no reconstruction compilation."""
from pathlib import Path
import subprocess
import tempfile
import local_data_ownership as audit


def run(original=None):
    count = 0
    with tempfile.TemporaryDirectory(prefix='local-owner-') as folder:
        root = Path(folder)
        def assemble(name, filename, body):
            obj = root / (name + '.o')
            subprocess.run(['as', '--32', '-o', str(obj)], input='.file "'+filename+'"\n'+body,
                           text=True, capture_output=True, check=True)
            return obj
        def function(name, target, pc=False):
            return '.text\n.globl %s\n.type %s,@function\n%s:\n.byte 0xb8\n.long %s%s\nret\n.size %s,.-%s\n' % (name,name,name,target,'-.' if pc else '',name,name)
        local = '.data\n.local slot\n.type slot,@object\nslot:\n.long 7\n.size slot,4\n.space 12\n'
        inputs = [assemble('a','dup.c',local+function('first','slot')),
                  assemble('b','dup.c',local+function('second','slot+1'))]
        linked = root/'linked.o'
        subprocess.run(['ld','-m','elf_i386','-r','-o',str(linked)]+list(map(str,inputs)),check=True,capture_output=True)
        linked_report = audit.build(linked)
        records = [r for r in linked_report['records'].values() if r['function']['bind']=='GLOBAL']
        assert len(records)==2 and all(r['status']=='unique-LOCAL-data-owner' for r in records)
        assert len({r['owner']['file_symtab_index'] for r in records})==2
        assert len({r['witnesses'][0]['object']['num'] for r in records})==2
        assert {r['owner']['file_occurrence'] for r in records}=={0,1}
        count += 1
        manifest = root/'manifest.txt'
        manifest.write_text('a.o src/alias.c\nb.o src/dup.c\n')
        screen = audit.source_screen(linked_report, manifest)
        assert len(screen['nominations'])==1 and screen['nominations'][0]['function']=='first'
        assert screen['nominations'][0]['size_delta']==0
        count += 1
        manifest.write_text('a.o src/dup.c\nb.o src/dup.c\n')
        assert not audit.source_screen(linked_report, manifest)['nominations']
        count += 1
        controls = {
            'one-past':local+function('probe','slot+4'),
            'gap':local+function('probe','slot+5'),
            'overlap':local+'.local alias\n.type alias,@object\n.set alias,slot\n.size alias,4\n'+function('probe','slot'),
            'global':local.replace('.local slot','.globl slot')+function('probe','slot'),
            'pc32':local+function('probe','slot',pc=True),
            'zero':local.replace('.size slot,4','.size slot,0')+function('probe','slot'),
            'linkonce':local+function('probe','slot').replace('.text\n','.section .gnu.linkonce.t.probe,"ax"\n'),
            'mergeable':local.replace('.data\n','.section .rodata.cst4,"aM",@progbits,4\n')+function('probe','slot'),
        }
        for name, body in controls.items():
            result = audit.build(assemble(name,'control.c',body))
            record, = [r for r in result['records'].values() if r['function']['name']=='probe']
            assert record['status']=='no-witness' and sum(record['rejections'].values())==1, name
            count += 1
        conflict = assemble('conflict','one.c',local+'.file "two.c"\n.data\n.local other\n.type other,@object\nother:\n.long 2\n.size other,4\n'+function('probe','slot').replace('ret\n','.long other\nret\n'))
        record, = list(audit.build(conflict)['records'].values())
        assert record['status']=='conflicting-LOCAL-data-owners' and record['owner'] is None
        count += 1
    if original:
        result = audit.build(original)
        record, = [r for r in result['records'].values() if r['function']['name']=='V32FP_modem']
        assert record['status']=='unique-LOCAL-data-owner' and record['owner']['file']=='V32mod.c'
        assert len(record['witnesses'])==4
        assert {w['object']['name'] for w in record['witnesses']}=={'tx_in_internal','rx_out_internal'}
        count += 1
        assert not any(w['object']['name']=='SMCv32_CFG' for r in result['records'].values() for w in r['witnesses'])
        count += 1
        checked = [r for r in result['records'].values() if r['owner'] and r['local_function_FILE_truth']]
        assert checked and all(r['owner']==r['local_function_FILE_truth'] for r in checked)
        print('original held-out LOCAL-function FILE agreement: %d/%d' % (len(checked),len(checked)))
        count += 1
    print('%d ELF-backed controls passed: duplicate FILE/local identities, source nominations positive/negative, one-past, gap, overlap, GLOBAL, PC32, zero-size, linkonce/mergeable, conflicting owners%s' %
          (count, '; original V32 positive/SMC exclusion/held-out locals' if original else ''))


if __name__ == '__main__':
    run()
