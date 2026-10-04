#!/usr/bin/env python3
"""Wave-2 round 2: dp_runtime_create tail permutation domain under the
confirmed baseline head (head-swap excluded: it folds the two flag ANDs
the blob keeps separate across the qcIndex branch).
"""
import playbook_small_patterns as driver

REV = '17d182af'
OUT_NAME = 'playbook-fdsp-dprt-r2'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/core/dp_param.c',)

CLK = '\trt->clockDeviation = info->clock_deviation;\n'
MODE = '\trt->modeFlags = 1;\n'
CONN = '\trt->connectionType = (int)info->connection_type;\n'
TAILBLOCK = CLK + MODE + CONN


def variants(path, source):
    assert path == 'src/core/dp_param.c'
    start = source.index('\ndp_runtime_create(') + 1
    end = source.index('\n}\n', start) + 3
    head, fn, tail = source[:start], source[start:end], source[end:]
    assert fn.count(TAILBLOCK) == 1
    cells = {'baseline': fn}
    for label, block in (('tail-mode-clk-conn', MODE + CLK + CONN),
                         ('tail-mode-conn-clk', MODE + CONN + CLK),
                         ('tail-conn-clk-mode', CONN + CLK + MODE),
                         ('tail-conn-mode-clk', CONN + MODE + CLK)):
        cells[label] = fn.replace(TAILBLOCK, block)
    assert len(cells) == len(set(cells.values())) == 5
    return {label: head + body + tail for label, body in cells.items()}


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
