#!/usr/bin/env python3
"""Test the observed post-consumption counter and a legal carried history index."""
import sys
import playbook_small_patterns as d


def variants(path, source):
    cells = {}
    for polyphase in (False, True):
        for circular in (False, True):
            text = source
            if polyphase:
                start, end, fn = d.function(text, "rc_output")
                old = "\tint k;\n\n\tfor (k = 0; k < s->taps; k++)\n\t\tacc += (int)coeff[k] * (int)hist[k];"
                assert fn.count(old) == 1
                new = ("\tunsigned int remaining = (unsigned int)s->taps;\n\n"
                       "\tfor (; remaining != 0; --remaining)\n"
                       "\t\tacc += (int)*coeff++ * (int)*hist++;")
                text = text[:start] + fn.replace(old, new) + text[end:]
            if circular:
                start, end, fn = d.function(text, "rc_circular_output")
                old = ("\tconst short *cursor = history + position + 1;\n\n"
                       "\tfor (i = position; i >= 0; i--)\n"
                       "\t\tacc += (int)*--cursor * (int)*coeff++;\n"
                       "\tcursor += count;\n"
                       "\tfor (i = count - 1; i > position; i--)\n"
                       "\t\tacc += (int)*--cursor * (int)*coeff++;")
                new = ("\n\tfor (i = position; i >= 0; i--)\n"
                       "\t\tacc += (int)history[i] * (int)*coeff++;\n"
                       "\ti += count;\n"
                       "\tfor (; i > position; i--)\n"
                       "\t\tacc += (int)history[i] * (int)*coeff++;")
                assert fn.count(old) == 1
                text = text[:start] + fn.replace(old, new) + text[end:]
            cells["baseline" if not polyphase and not circular else "polyphase-%d-circular-%d" % (polyphase, circular)] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == "__main__":
    assert "--domain" in sys.argv
    d.REV = "a5f306a3"
    d.OUT_NAME = "recovery-fixedrc-counter-epoch"
    d.SOURCE_PATHS = ("src/core/FixedRC.c",)
    d.DUMP_FLAGS = ("-v", "-save-temps", "-da")
    d.variants = variants
    d.main()
