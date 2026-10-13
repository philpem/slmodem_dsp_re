#!/usr/bin/env python3
"""Reproduce PSD rename decisions with saved/plain/observed raw controls."""
import hashlib
import json
import shlex
import shutil
import subprocess
import playbook_small_patterns as d
import experiment_toolchain as tc


def run(command, folder, name):
    with (folder/(name+'.log')).open('w') as stream:
        subprocess.run(command,cwd=folder,stdout=stream,stderr=subprocess.STDOUT,check=True)


def main():
    source = d.ROOT/'build/gcc3-psd-conversion-plain/psd/baseline'
    root = d.ROOT/'build/gcc3-psd-rename-observation'
    root.mkdir(exist_ok=True)
    compiler = root/'cc1plus'
    shutil.copyfile(d.ROOT/'build/gcc3-psd-conversion-plain-observation/cc1plus',compiler)
    compiler.chmod(0o755)
    shutil.copyfile(source/'psd.ii',root/'psd.ii')
    lines = (source/'compile.log').read_text().splitlines()
    command = shlex.split(next(line for line in lines if '/cc1plus -fpreprocessed ' in line))
    command[0] = str(compiler)
    command[command.index('-o')+1] = 'plain.s'
    run(command,root,'plain')
    observed = list(command)
    observed[observed.index('-o')+1] = 'observed.s'
    debugger = ['gdb','-q','-batch','-ex','set pagination off','-ex',
                'source '+str(d.ROOT/'tools/gcc3_psd_rename_gdb.py'),'--args']+observed
    run(debugger,root,'gdb')
    assert (source/'psd.s').read_bytes() == (root/'plain.s').read_bytes() == (root/'observed.s').read_bytes()
    assembler = shlex.split(next(line for line in lines if '/bin/as ' in line))
    assembler[assembler.index('-o')+1] = '/work/observed.o'
    assembler[-1] = 'observed.s'
    config = (d.ROOT/'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ',1)[1]
    assemble = tc.docker_prefix(image,d.ROOT,root,True)+['/bin/sh','-c','cd /work && '+shlex.join(assembler)]
    run(assemble,root,'assemble')
    assert (root/'observed.o').read_bytes() == (source/'candidate.o').read_bytes()
    trace = json.loads((root/'rename-observe.json').read_text())
    assert not trace['errors'] and trace['exit_codes'] == [0] and trace['events']
    report = {'revision':'5cd55f7a','config':config,'commands':[command,debugger,assemble],
              'object_sha256':hashlib.sha256((root/'observed.o').read_bytes()).hexdigest(),
              'raw_assembly_object_equal':True,'trace':trace}
    (root/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('%d process rename decisions; raw assembly/object equal' % len(trace['events']))


if __name__ == '__main__':
    main()
