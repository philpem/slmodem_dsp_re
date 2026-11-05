#!/usr/bin/env python3
"""Screen immediate-to-memory stores for unmatched narrow-to-wide widths.

This reports instruction-pattern candidates only. It does not prove the source
field owner, access semantics, or that a printed displacement names the same
embedded object on both sides.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
import dis  # initialize stdlib before adding tools to sys.path
sys.path.insert(0, str(TOOLS / 'toolchain'))
import byteident
import jumptable

WIDTHS = ('b', 'w', 'l', 'd')
NARROW = frozenset(('b', 'w'))
WIDE = frozenset(('l', 'd'))
MNEMONIC = re.compile(r'mov([bwld])\Z')
IMMEDIATE = re.compile(r'\$(-?(?:0x[0-9a-fA-F]+|[0-9]+))\Z')
DISPLACED_MEMORY = re.compile(
    r'-?(?:0x[0-9a-fA-F]+|[0-9]+)\([^()]*\)\Z')


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


ANALYSIS_FILES = (Path(__file__), TOOLS / 'toolchain/byteident.py',
                  TOOLS / 'toolchain/jumptable.py',
                  TOOLS / 'toolchain/induction_jumptable.py',
                  TOOLS / 'toolchain/readonly_copy.py')


def analysis_hashes():
    return {path.name: sha256(path) for path in ANALYSIS_FILES}


_LOADED_ANALYSIS_DIGESTS = analysis_hashes()


def _clear_byteident_caches():
    """Width rows must decode this object version, not a stale path-cache hit."""
    for name in ('section_symbols', 'function_owners', 'body'):
        getattr(byteident, name).cache_clear()
    for name in ('instructions', 'section_direct_edges'):
        getattr(jumptable, name).cache_clear()


def immediate_memory_stores(path, symbol):
    """Return parsed mov[bwld] immediate-to-displaced-memory stores."""
    stores = []
    for mnemonic, operands in byteident.insns(str(path), symbol):
        match = MNEMONIC.fullmatch(mnemonic)
        if match is None:
            continue
        parts = operands.split(',', 1)
        if len(parts) != 2:
            continue
        source, destination = (part.strip() for part in parts)
        immediate = IMMEDIATE.fullmatch(source)
        if immediate is None or DISPLACED_MEMORY.fullmatch(destination) is None:
            continue
        immediate_text = immediate.group(1)
        if immediate_text.startswith(('0x', '-0x')):
            value = int(immediate_text, 16)
        else:
            value = int(immediate_text, 10)
        stores.append({'width': match.group(1),
                       'immediate': value,
                       'destination': destination,
                       'mnemonic': mnemonic, 'operands': operands})
    return stores


def unmatched_narrow_to_wide(native_stores, candidate_stores):
    """Cancel same-signature stores at each width before testing multiplicity."""
    def counts(stores):
        return Counter((row['immediate'], row['destination'], row['width'])
                       for row in stores)

    native, candidate = counts(native_stores), counts(candidate_stores)
    signatures = sorted({(value, destination)
                         for value, destination, _ in native.keys() | candidate.keys()},
                        key=lambda row: (row[0], row[1]))
    findings = []
    for value, destination in signatures:
        canceled = {width: min(native[(value, destination, width)],
                               candidate[(value, destination, width)])
                    for width in WIDTHS}
        left_native_narrow = {width: native[(value, destination, width)] - canceled[width]
                              for width in NARROW}
        left_candidate_wide = {width: candidate[(value, destination, width)] - canceled[width]
                               for width in WIDE}
        native_count = sum(left_native_narrow.values())
        candidate_count = sum(left_candidate_wide.values())
        if native_count > 0 and native_count == candidate_count:
            findings.append({'signature': {'immediate': value, 'destination': destination},
                             'canceled_same_width': canceled,
                             'unmatched_native_narrow': left_native_narrow,
                             'unmatched_candidate_wide': left_candidate_wide,
                             'multiplicity': native_count,
                             'owner_proven': False})
    return findings


def analyze(native_object, candidate_object, symbols=None, max_bytes=None):
    """Compare explicit object paths; C and C++ symbols share the same path."""
    native_object = str(Path(native_object).resolve())
    candidate_object = str(Path(candidate_object).resolve())
    for path in (native_object, candidate_object):
        if not Path(path).is_file():
            raise ValueError('missing object: ' + path)
    input_digests = {path: sha256(path) for path in {native_object, candidate_object}}
    analysis_path = str(Path(__file__).resolve())
    analysis_digests = analysis_hashes()
    analysis_digest = analysis_digests[Path(__file__).name]
    if analysis_digests != _LOADED_ANALYSIS_DIGESTS:
        raise ValueError('analysis script changed since import; restart Python')
    if max_bytes is not None and max_bytes < 0:
        raise ValueError('--max-bytes must be nonnegative')
    native_sizes = byteident.sizes(native_object)
    candidate_sizes = byteident.sizes(candidate_object)
    shared = set(native_sizes) & set(candidate_sizes)
    if symbols is None:
        selected = shared
        missing = []
    else:
        requested = sorted(set(symbols))
        selected = set(requested) & shared
        missing = sorted(set(requested) - shared)
    before_size_filter = set(selected)
    if max_bytes is not None:
        selected = {name for name in selected if native_sizes[name] <= max_bytes}
    excluded_by_max_bytes = sorted(before_size_filter - selected)
    if not selected:
        raise ValueError('selected shared-function denominator is 0')

    _clear_byteident_caches()
    rows = []
    for symbol in sorted(selected):
        native = immediate_memory_stores(native_object, symbol)
        candidate = immediate_memory_stores(candidate_object, symbol)
        findings = unmatched_narrow_to_wide(native, candidate)
        rows.append({'symbol': symbol, 'native_size': native_sizes[symbol],
                     'candidate_size': candidate_sizes[symbol],
                     'native_store_count': len(native), 'candidate_store_count': len(candidate),
                     'findings': findings,
                     'native_stores': native, 'candidate_stores': candidate})
    for path, before in input_digests.items():
        try:
            after = sha256(path)
        except OSError as exc:
            raise ValueError('input changed during width analysis: ' + path) from exc
        if after != before:
            raise ValueError('input changed during width analysis: ' + path)
    if analysis_hashes() != analysis_digests:
        raise ValueError('analysis script changed during width analysis; restart Python')
    return {'schema': 'gcc3-native-store-width-screen-v1', 'status': 'valid',
            'analysis_script': {'path': analysis_path, 'sha256': analysis_digest},
            'analysis_sha256': analysis_digests,
            'inputs': {'native': {'path': native_object, 'sha256': input_digests[native_object]},
                       'candidate': {'path': candidate_object, 'sha256': input_digests[candidate_object]}},
            'denominator': {'shared_functions': len(shared), 'selected_functions': len(selected),
                            'missing_requested_symbols': missing,
                            'excluded_by_max_bytes': excluded_by_max_bytes,
                            'max_bytes': max_bytes},
            'candidate_count': sum(len(row['findings']) for row in rows),
            'owner_proven': False, 'functions': rows,
            'scope': 'Immediate-to-displaced-memory mov stores with equal numeric constant and '
                     'complete printed destination. Same-width multiplicities cancel first; only '
                     'equal residual native b/w and candidate l/d counts are reported. This is a '
                     'printed instruction-pattern screen, not source-owner or semantic proof.'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', required=True, type=Path,
                        help='native/reference relocatable object path')
    parser.add_argument('--candidate', required=True, type=Path,
                        help='candidate relocatable object path')
    parser.add_argument('--symbol', action='append',
                        help='limit to this symbol (repeatable); otherwise scan shared functions')
    parser.add_argument('--max-bytes', type=int,
                        help='select symbols whose native function size is at most this many bytes')
    parser.add_argument('--output', required=True, type=Path,
                        help='new JSON report path; existing files are not overwritten')
    args = parser.parse_args(argv)
    stream = None
    try:
        stream = args.output.open('x')
        result = analyze(args.native, args.candidate, args.symbol, args.max_bytes)
        json.dump(result, stream, indent=2, sort_keys=True)
        stream.write('\n')
        print('valid: %d selected functions, %d width candidates; owner proven: false' %
              (result['denominator']['selected_functions'], result['candidate_count']))
        return 0
    except (OSError, ValueError, RuntimeError) as exc:
        if stream is not None:
            stream.seek(0)
            stream.truncate()
            json.dump({'status': 'invalid', 'denominator': None, 'error': str(exc)}, stream)
            stream.write('\n')
        print('invalid: denominator not established: ' + str(exc), file=sys.stderr)
        return 1
    finally:
        if stream is not None:
            stream.close()


if __name__ == '__main__':
    raise SystemExit(main())
