#!/usr/bin/env python3
"""Compile unchanged complete inventory TUs with production GCC3 diagnostics."""
import argparse
import json
from pathlib import Path
import sys
import playbook_small_patterns as d
import experiment_toolchain as tc


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--source', action='append', help='optional first-wave source selection')
    parser.add_argument('--exclude-source', action='append', default=[], help='explicit failed dump source handled in a separate control')
    parser.add_argument('--source-list', type=Path, help='JSON list of remaining complete source TUs')
    parser.add_argument('--domain', required=True)
    parser.add_argument('--focused-dumps', action='store_true', help='use the previously working ADID r/c/R/S diagnostic subset')
    parser.add_argument('--no-asm-patterns', action='store_true', help='omit diagnostic -dP after a preserved compiler dump failure')
    parser.add_argument('--output-name', default='residual-stage-baselines')
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text())
    sources = sorted({copy['source'] for row in inventory['candidates'] for copy in row['definitions']})
    selected = (args.source or []) + (json.loads(args.source_list.read_text()) if args.source_list else [])
    if selected:
        assert set(selected) <= set(sources), 'source outside inventory'
        sources = sorted(set(selected))
    assert set(args.exclude_source) <= set(sources)
    sources = [source for source in sources if source not in args.exclude_source]
    assert sources
    assert len({Path(path).stem for path in sources}) == len(sources), 'driver family-name collision'
    config = (d.ROOT/'build/tc_out/.build-config').read_text()
    assert config == inventory['build_config']
    tc.print_identity(config.splitlines()[0].split(' ', 1)[1], tc.GENTOO_COMPILER_PATH, True)
    d.REV = inventory['revision']
    d.OUT_NAME = args.output_name
    d.SOURCE_PATHS = tuple(sources)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da') + (() if args.no_asm_patterns else ('-dP',))
    if args.focused_dumps:
        d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dR', '-dS')
    d.variants = lambda path, source: {'baseline': source}
    print(len(sources), 'unchanged complete TUs selected', flush=True)
    sys.argv = [sys.argv[0], '--domain', args.domain]
    d.main()


if __name__ == '__main__':
    main()
