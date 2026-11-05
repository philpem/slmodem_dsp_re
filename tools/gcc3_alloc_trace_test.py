#!/usr/bin/env python3
"""Positive and refusal controls; explicit checks remain active under Python -O."""
import copy
from gcc3_alloc_trace import validate_trace


def fixture():
    return {'errors': [], 'exit_codes': [0], 'pending': 0,
            'denominators': {'target_local_choices': 1, 'whole_tu_local_choices': 2,
                             'target_pass_entries': 2, 'whole_tu_pass_entries': 4},
            'events': [
                {'sequence': 0, 'kind': 'pass_entry', 'pass': 'local_alloc'},
                {'sequence': 1, 'kind': 'local_choice', 'return': 2, 'exclusions': [{'used': [1]}]},
                {'sequence': 2, 'kind': 'local_choice_return', 'choice_sequence': 1, 'selected': 2},
                {'sequence': 3, 'kind': 'pass_entry', 'pass': 'global_alloc'}]}


def main():
    if validate_trace(fixture())['target_local_choices'] != 1:
        raise RuntimeError('positive control failed')
    def set_error(trace): trace['errors'].append('required field unavailable')
    def pending(trace): trace['pending'] = 1
    def denominator(trace): trace['denominators']['whole_tu_local_choices'] = 0
    def missing_target(trace): trace['denominators']['target_local_choices'] = 0
    def missing_return(trace): trace['events'].pop(2)
    def missing_exclusion(trace): trace['events'][1]['exclusions'] = []
    def wrong_return(trace): trace['events'][2]['selected'] = 3
    def wrong_pass(trace): trace['events'][3]['pass'] = 'reload'
    def failed_exit(trace): trace['exit_codes'] = [1]
    passed = 1
    for mutation in (set_error, pending, denominator, missing_target, missing_return,
                     missing_exclusion, wrong_return, wrong_pass, failed_exit):
        trace = copy.deepcopy(fixture())
        mutation(trace)
        try:
            validate_trace(trace)
        except RuntimeError:
            passed += 1
        else:
            raise RuntimeError('invalid trace accepted: ' + mutation.__name__)
    print('%d/10 allocation trace validation controls passed' % passed)


if __name__ == '__main__':
    main()
