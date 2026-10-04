#!/usr/bin/env python3
"""Wave-2 byte-exactness: FDSP_DP_Delete tail spelling + dp_runtime_create order.

Posted as the wave-2 pre-compile declaration in #22. All cells reorder
behavior-identical statements; no fuzzing or mutation execution.
"""
import playbook_small_patterns as driver

REV = '17d182af'
OUT_NAME = 'playbook-fdsp-dprt'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/service/Fdsp.c', 'src/core/dp_param.c')

# A: FDSP_DP_Delete tail (src/service/Fdsp.c)
FREE_K = '\tsysdep_free(k);\n'
ZERO_GLOBAL = '\tpGlobalFDSPObj = 0;\n'

# B: dp_runtime_create (src/core/dp_param.c)
QCINDEX = '\trt->qcIndex = info->qc_index ? (int)info->qc_index : 9;\n'
UNNAMED3 = '\trt->unnamed_0003 &= (unsigned char)~0x07;\n'
QCFLAGS80 = '\trt->qcFlags &= (unsigned char)~0x80;\n'
CLK = '\trt->clockDeviation = info->clock_deviation;\n'
MODE = '\trt->modeFlags = 1;\n'
CONN = '\trt->connectionType = (int)info->connection_type;\n'
TAILBLOCK = CLK + MODE + CONN


def fdsp_variants(fn):
    old = FREE_K + ZERO_GLOBAL
    assert fn.count(old) == 1
    cells = {'baseline': fn}
    cells['k-zero-store'] = fn.replace(
        old, FREE_K + '\tk = 0;\n\tpGlobalFDSPObj = k;\n')
    cells['chained-zero'] = fn.replace(
        old, FREE_K + '\tpGlobalFDSPObj = k = 0;\n')
    cells['comma-form'] = fn.replace(
        old, '\tsysdep_free(k), k = 0, pGlobalFDSPObj = k;\n')
    assert len(cells) == len(set(cells.values())) == 4
    return cells


def dprt_variants(fn):
    assert all(fn.count(x) == 1 for x in
               (QCINDEX, UNNAMED3, QCFLAGS80, CLK, MODE, CONN))
    assert fn.count(TAILBLOCK) == 1
    # head-swap: qcIndex moves after the final qcFlags clear (75, 76, 74).
    head = fn.replace(QCINDEX, '')
    head = head.replace(QCFLAGS80, QCFLAGS80 + QCINDEX)
    assert head != fn
    ta = CLK + CONN + MODE
    tb = CONN + CLK + MODE
    cells = {'baseline': fn,
             'head-swap': head,
             'head+clk-conn-mode': head.replace(TAILBLOCK, ta),
             'head+conn-clk-mode': head.replace(TAILBLOCK, tb),
             'tail-only+clk-conn-mode': fn.replace(TAILBLOCK, ta)}
    assert len(cells) == len(set(cells.values())) == 5
    return cells


def variants(path, source):
    if path == 'src/service/Fdsp.c':
        start = source.index('\nFDSP_DP_Delete(') + 1
        end = source.index('\n}\n', start) + 3
        head, fn, tail = source[:start], source[start:end], source[end:]
        cells = fdsp_variants(fn)
        return {label: head + body + tail for label, body in cells.items()}
    start = source.index('\ndp_runtime_create(') + 1
    end = source.index('\n}\n', start) + 3
    head, fn, tail = source[:start], source[start:end], source[end:]
    cells = dprt_variants(fn)
    return {label: head + body + tail for label, body in cells.items()}


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
