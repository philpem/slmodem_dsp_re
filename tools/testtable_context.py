#!/usr/bin/env python3
"""Prepare test-only D302 and FP_Pow shared or isolated memory environments.

Published copies change mtime only when their ELF bytes change. Source/repro
inputs, actual table definitions and all fixture table comparisons stay intact.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import tempfile
from test_table_binding import bind_table_read, _locate, _read, _unique, require


def default(source, table_object, output):
    return bind_table_read(source, 'FSE_decision_16pt', 'DECv32_MAG9600',
                           'ref_DECv32_MAG9600', output, table_object=table_object)


def d302(source, reference, directory):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    return [bind_table_read(source, 'FSE_decision_16pt', 'ref_DECv32_MAG9600',
                            'd302_backing', directory / 'source.o', table_object=reference),
            bind_table_read(reference, 'ref_FSE_decision_16pt', 'ref_DECv32_MAG9600',
                            'd302_backing', directory / 'ref.o')]


def fp_pow_reference(reference):
    """Verify the historical six-word table and its actual seventh word."""
    _locate(reference, 'ref_FP_Pow', 'ref_Fact_FP', (0,), profile='fp-pow')
    raw, elf, _, symbols = _read(reference)
    _, table = _unique(symbols, 'ref_Fact_FP')
    _, adjacent = _unique(symbols, 'ref_IIRFilterScales')
    require(table['st_info']['bind'] == 'STB_GLOBAL' and
            adjacent['st_info']['bind'] == 'STB_GLOBAL' and
            adjacent['st_info']['type'] == 'STT_OBJECT' and
            adjacent['st_size'] == 8 and
            adjacent['st_shndx'] == table['st_shndx'] and
            adjacent['st_value'] == table['st_value'] + 12,
            'FP_Pow reference requires adjacent eight-byte IIRFilterScales')
    section = elf.get_section(table['st_shndx'])
    require(section['sh_flags'] & 2 and not section['sh_flags'] & 4 and
            section['sh_type'] != 'SHT_NOBITS' and
            adjacent['st_value'] + adjacent['st_size'] <= section['sh_size'],
            'FP_Pow reference table/neighbor data extent')
    require(struct.unpack_from('<h', section.data(), adjacent['st_value'])[0] == 3,
            'FP_Pow historical seventh word must be 3')
    return {'path': str(Path(reference).resolve()),
            'sha256': hashlib.sha256(raw).hexdigest(),
            'table': 'ref_Fact_FP', 'table_size': 12,
            'adjacent_symbol': 'ref_IIRFilterScales', 'seventh_word': 3}


def fp_pow(source, reference, output):
    """Give candidate reads the original table's historical memory context."""
    require(Path(output).resolve() != Path(reference).resolve(),
            'context output must differ from authoritative reference')
    context = fp_pow_reference(reference)
    record = bind_table_read(source, 'FP_Pow', 'Fact_FP', 'ref_Fact_FP', output,
                             allowed_addends=(0,), profile='fp-pow')
    record['historical_reference'] = context
    return record


def fp_pow_seventh(source, reference, directory):
    """Bind the normal bound source and native reads to a seven-word fixture.

    The linked fixture defines fp_pow_probe_backing[7]. Both object rewrites
    must validate before either result is published; failed validation leaves
    any existing destination pair intact.
    """
    context = fp_pow_reference(reference)
    directory = Path(directory).resolve()
    require(Path(source).resolve() not in (directory/'source.o', directory/'ref.o') and
            Path(reference).resolve() not in (directory/'source.o', directory/'ref.o'),
            'context outputs must differ from both inputs')
    directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=directory) as temporary:
        staged = Path(temporary)
        records = [bind_table_read(source, 'FP_Pow', 'ref_Fact_FP',
                                  'fp_pow_probe_backing', staged/'source.o',
                                  allowed_addends=(0,), table_object=reference, profile='fp-pow'),
                   bind_table_read(reference, 'ref_FP_Pow', 'ref_Fact_FP',
                                  'fp_pow_probe_backing', staged/'ref.o',
                                  allowed_addends=(0,), profile='fp-pow')]
        for name, record in zip(('source.o', 'ref.o'), records):
            output = directory/name
            if not output.exists() or output.read_bytes() != (staged/name).read_bytes():
                (staged/name).replace(output)
            record['output'] = str(output)
            record['historical_reference'] = context
            record['fixture_contract'] = 'fp_pow_probe_backing[7], six original coefficients and a variable seventh signed short'
    return records


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    ordinary = sub.add_parser('default')
    for name in ('source', 'table_object', 'output'):
        ordinary.add_argument(name)
    isolated = sub.add_parser('d302')
    for name in ('source', 'reference', 'directory'):
        isolated.add_argument(name)
    fp = sub.add_parser('fp-pow')
    for name in ('source', 'reference', 'output'):
        fp.add_argument(name)
    seventh = sub.add_parser('fp-pow-seventh')
    for name in ('source', 'reference', 'directory'):
        seventh.add_argument(name)
    args = vars(parser.parse_args())
    mode = args.pop('mode')
    record = {'default': default, 'd302': d302, 'fp-pow': fp_pow,
              'fp-pow-seventh': fp_pow_seventh}[mode](**args)
    print(json.dumps(record, indent=2))
