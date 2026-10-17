#!/usr/bin/env python3
"""Replay unchanged full TU and generate one explicit wrong-scale ELF canary.

The canary changes the LE reciprocal from 2**-24 to 2**-23. It is measurement
apparatus, never a source hypothesis or an adoptable compiler option.
"""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile

if __name__=='__main__':
    d.REV='95422cd2'
    d.OUT_NAME='recovery-equalizer-numeric'
    d.SOURCE_PATHS=('src/pump/v90/V90Equalizer.cpp',)
    d.DUMP_FLAGS=()
    d.variants=lambda path,source:{'baseline':source}
    d.main()
    folder=d.ROOT/'build'/d.OUT_NAME/'V90Equalizer/baseline'
    path=folder/'candidate.o';raw=bytearray(path.read_bytes())
    with path.open('rb') as stream:
        elf=ELFFile(stream);pool=elf.get_section_by_name('.rodata.cst4')
        original=b'\x00\x00\x80\x33';changed=b'\x00\x00\x00\x34'
        assert pool.data().count(original)==1
        relative=pool.data().index(original);at=pool['sh_offset']+relative
    assert raw[at:at+4]==original
    raw[at:at+4]=changed
    wrong=folder.parent/'wrong-scale.o';wrong.write_bytes(raw)
    (folder.parent/'wrong-scale.json').write_text(json.dumps({'baseline_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'wrong_sha256':hashlib.sha256(raw).hexdigest(),'section':'.rodata.cst4','section_offset':relative,
        'before':original.hex(),'after':changed.hex(),'scope':'deliberately wrong LE-scale calibration object, never production'},indent=2)+'\n')
    print('one actual ELF wrong-scale canary generated:',wrong)
