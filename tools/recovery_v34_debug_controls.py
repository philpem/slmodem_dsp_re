#!/usr/bin/env python3
"""Run seven period-assembler controls for the V34 diagnostic resolver."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / 'build/v34-root-closure293'
CASES = {
    'positive': 'movl $fmt,%eax\nmovl %eax,(%esp)',
    'null-format': 'movl $fmt,%eax\nmovl $0,(%esp)',
    'removed-format': 'movl $fmt,%eax\nnop',
    'call-clobber': 'movl $fmt,%eax\ncall external_clobber\nmovl %eax,(%esp)',
    'partial-register': 'movl $fmt,%eax\nxorw %ax,%ax\nmovl %eax,(%esp)',
    'esp-adjust': 'movl $fmt,(%esp)\nsubl $4,%esp',
    'indirect-entry': 'xorl %eax,%eax\njmp *table(,%eax,4)\nmovl $fmt,(%esp)\njoin:',
}

def run(out, config_path):
    import experiment_toolchain as tc
    import recovery_v34_debug_calls as resolver

    config = config_path.read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    out.mkdir(parents=True, exist_ok=True)
    report = {'config_path': str(config_path), 'config': config,
              'config_sha256': hashlib.sha256(config.encode()).hexdigest(),
              'commands': [], 'cases': {}}
    identity = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
        'export PATH=' + tc.GENTOO_COMPILER_PATH + ':$PATH; gcc --version; p=$(gcc -print-prog-name=as); "$p" --version']
    with (out / 'identity.log').open('w') as log:
        subprocess.run(identity, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    report['identity_command'] = identity
    for label, body in CASES.items():
        tail = '\n.section .rodata\ntable: .long join\n' if label == 'indirect-entry' else ''
        text = ('.text\n.globl v34handshak\n.type v34handshak,@function\nv34handshak:\nsubl $32,%esp\n'
                + body + '\ncall dsplibs_debug_printf\naddl $32,%esp\nret\n.size v34handshak,.-v34handshak\n'
                + '.section .rodata.str1.1,"aMS",@progbits,1\nfmt: .asciz "known diagnostic\\n"\n' + tail)
        (out / (label + '.s')).write_text(text)
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
            tc.compile_shell(tc.GENTOO_COMPILER_PATH, [], '/work/' + label + '.o', '/work/' + label + '.s')]
        report['commands'].append(command)
        with (out / (label + '.compile.log')).open('w') as log:
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        result = resolver.audit(out / (label + '.o'))
        assert result['denominator'] == 1
        assert (result['resolved'] == 1) == (label == 'positive'), (label, result)
        result['source_sha256'] = hashlib.sha256(text.encode()).hexdigest()
        report['cases'][label] = result
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
        print(label, result['resolved'], '/', result['denominator'], result['calls'][0]['arguments']['0'], flush=True)
    print('seven controls: 1 positive, 6 negatives passed', flush=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=DEFAULT / 'resolver-controls', help='directory for control sources, objects and logs')
    parser.add_argument('--config', type=Path, default=DEFAULT / 'frozen/.build-config', help='frozen period build configuration')
    args = parser.parse_args()
    run(args.out.resolve(), args.config.resolve())

if __name__ == '__main__':
    main()
