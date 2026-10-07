#!/usr/bin/env python3
"""Cross the original ordered state-group switch with its shared result."""
import playbook_small_patterns as d
from fax_status_common_result_reproduce import variants as prior


def variants(path, source):
    cells = prior(path, source)
    start, end, function = d.function(source, 'fax_class1_status')
    a = function.index('\tif (ctx->state >= 4')
    for shared in (False, True):
        body = function[:a]
        if shared:
            body = body.replace('{\n', '{\n\tint result = 0;\n', 1)
        body += '\tswitch (ctx->state) {\n\tcase CLASS1_HDLC_RECEIVE_LOOK_CARRIER_STATE:\n\tcase CLASS1_HDLC_RECEIVE_STATE:\n\tcase CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE:\n\t\tFAXVMI_status(ctx->vmi_a, &st);\n'
        body += '\t\tresult = 1;\n\t\tbreak;\n' if shared else '\t\treturn 1;\n'
        body += '\tcase CLASS1_RX_LOOK_CARRIER:\n\tcase CLASS1_RX_DATA_STATE:\n\t\tFAXVMI_status(ctx->vmi_b, &st);\n'
        body += '\t\tresult = 1;\n\t\tbreak;\n' if shared else '\t\treturn 1;\n'
        body += '\t}\n\treturn '+('result' if shared else '0')+';\n}'
        cells['switch-'+('shared' if shared else 'early')] = source[:start]+body+source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = '9025b8d8'
    d.SOURCE_PATHS = ('src/fax/class1.c',)
    d.OUT_NAME = 'fax-status-dispatch'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
