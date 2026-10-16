#!/usr/bin/env python3
"""Cross witnessed unsigned countdown and forward cursors in generic RcFixed FIR."""
import sys
import playbook_small_patterns as d


def variants(path, source):
    start, end, fn = d.function(source, "rc_output")
    old = "\tint k;\n\n\tfor (k = 0; k < s->taps; k++)\n\t\tacc += (int)coeff[k] * (int)hist[k];"
    assert fn.count(old) == 1
    cells = {}
    for countdown in (False, True):
        for cursors in (False, True):
            if not countdown and not cursors:
                cells["baseline"] = source
                continue
            if countdown:
                decl = "\tunsigned int remaining = (unsigned int)s->taps;\n"
                if not cursors:
                    decl += "\tunsigned int taps = remaining;\n"
                head = "\n\twhile (remaining-- != 0)"
                index = "taps - remaining - 1"
            else:
                decl = "\tint k;\n"
                head = "\n\tfor (k = 0; k < s->taps; k++)"
                index = "k"
            operands = "(int)*coeff++ * (int)*hist++" if cursors else "(int)coeff[%s] * (int)hist[%s]" % (index, index)
            replacement = decl + head + "\n\t\tacc += " + operands + ";"
            text = source[:start] + fn.replace(old, replacement) + source[end:]
            cells["countdown-%d-cursors-%d" % (countdown, cursors)] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == "__main__":
    assert "--domain" in sys.argv
    d.REV = "a5f306a3"
    d.OUT_NAME = "recovery-fixedrc-polyphase-loop"
    d.SOURCE_PATHS = ("src/core/FixedRC.c",)
    d.DUMP_FLAGS = ("-v", "-save-temps", "-da")
    d.variants = variants
    d.main()
