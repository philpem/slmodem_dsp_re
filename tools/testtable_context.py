#!/usr/bin/env python3
"""Prepare test-only shared-table and isolated D302 memory environments.

Published copies change mtime only when their ELF bytes change. Source/repro
inputs, actual table definitions and all fixture table comparisons stay intact.
"""
import argparse
import json
from pathlib import Path
from test_table_binding import bind_table_read


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


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    ordinary = sub.add_parser('default')
    for name in ('source', 'table_object', 'output'):
        ordinary.add_argument(name)
    isolated = sub.add_parser('d302')
    for name in ('source', 'reference', 'directory'):
        isolated.add_argument(name)
    args = vars(parser.parse_args())
    mode = args.pop('mode')
    record = default(**args) if mode == 'default' else d302(**args)
    print(json.dumps(record, indent=2))
