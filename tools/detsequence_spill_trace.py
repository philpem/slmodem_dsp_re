#!/usr/bin/env python3
"""Replay the spill-slot trace on two unchanged DetSequence full-TU controls."""
import detsequence_allocation_trace as trace
import json
import sys


SPILL_SCRIPT = r'''
class SpillReturn(gdb.FinishBreakpoint):
    def __init__(self, frame, number, source, before):
        super().__init__(frame, internal=True)
        self.pseudo, self.source, self.before = number, source, before
    def stop(self):
        after = int(gdb.parse_and_eval('cfun->x_frame_offset'))
        if after != self.before:
            print('SPILL_TRACE ' + json.dumps({'pseudo': self.pseudo,
                  'from_reg': self.source, 'frame_before': self.before,
                  'frame_after': after}))
        return False
class SpillEntry(gdb.Breakpoint):
    def stop(self):
        if target():
            # Read i386 cdecl arguments at the true function entry. Optimized
            # DWARF source-parameter locations can already describe a later PC.
            SpillReturn(gdb.newest_frame(), int(gdb.parse_and_eval('*(int*)($esp+4)')),
                        int(gdb.parse_and_eval('*(int*)($esp+8)')),
                        int(gdb.parse_and_eval('cfun->x_frame_offset')))
        return False
spill_breakpoint = None
def ensure_spill_breakpoint():
    global spill_breakpoint
    if spill_breakpoint is None:
        spill_breakpoint = SpillEntry('*alter_reg', internal=True)
'''


def analyze(out, domain):
    original_analyze(out, domain)
    result = json.loads((out / 'results.json').read_text())
    for label, row in result['cells'].items():
        log = (out / 'V32prc' / label / 'trace.log').read_text()
        events = [json.loads(line[len('SPILL_TRACE '):]) for line in log.splitlines()
                  if line.startswith('SPILL_TRACE ')]
        expected = [64, 67, 68, 69, 70, 72, 97, 102] if label == 'baseline' else [
            67, 68, 69, 70, 72, 96, 101, 66]
        assert [e['pseudo'] for e in events] == expected, 'spill-order control failed'
        assert [e['frame_after'] for e in events] == list(range(-4, -33, -4))
        row['spill_events'] = events
        print(label, 'spill order:', expected, '8 / 8')
    (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    original_analyze = trace.analyze
    trace.analyze = analyze
    trace.output_directory = lambda root, family: root / 'build/detsequence-spill-trace'
    trace.GDB_SCRIPT = trace.GDB_SCRIPT.replace("AllocationEntry('global_alloc', internal=True)",
                            SPILL_SCRIPT + "\nAllocationEntry('global_alloc', internal=True)")
    trace.GDB_SCRIPT = trace.GDB_SCRIPT.replace("if target(): snapshot('global-entry')",
                            "if target(): snapshot('global-entry'); ensure_spill_breakpoint()")
    sys.argv += ['--source-family', 'playbook-detsequence-shift-lifetime']
    trace.main()
