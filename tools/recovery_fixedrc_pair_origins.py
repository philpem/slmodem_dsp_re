#!/usr/bin/env python3
"""Cross the prior inline-origin control with new FixedRC carrier/owner evidence."""
import sys
import playbook_small_patterns as d
from recovery_fixedrc_wrap_owner import variants as owner_variants


def variants(path, source):
    selected = owner_variants(path, source)["pair-1-owner-1"]
    cells = {"baseline": source}
    for kind0 in (False, True):
        for kind1 in (False, True):
            start, end, fn = d.function(selected, "RcFixed_Resample")
            helpers = []
            for kind, enabled, state, name in (
                (0, kind0, "rc_state", "rc_polyphase_resample"),
                (1, kind1, "rc_kind1", "rc_cascade_resample"),
            ):
                if not enabled:
                    continue
                begin = fn.index("\n\tcase %d: {\n" % kind)
                finish = fn.index("\n\tdefault:", begin) if kind == 1 else fn.index("\n\tcase 1: {\n", begin)
                block = fn[begin:finish]
                lines = block.splitlines()
                assert lines[1] == "\tcase %d: {" % kind
                assert lines[-2:] == ["\t\tbreak;", "\t}"]
                assert lines[2].startswith("\t\tstruct " + state + " *s = ")
                body = "\n".join(line[1:] if line.startswith("\t") else line for line in lines[3:-2])
                helpers.append("static inline void\n" + name + "(struct " + state + " *s, const short *in, int in_count, short *out, int *out_count)\n{\n" + body + "\n}\n\n")
                call = ("(struct rc_kind1 *)h->state" if kind else "h->state")
                replacement = "\n\tcase %d:\n\t\t%s(%s, in, in_count, out, out_count);\n\t\tbreak;\n" % (kind, name, call)
                fn = fn[:begin] + replacement + fn[finish:]
            insertion = selected[:start].rfind("\nvoid\n")
            assert insertion >= 0
            text = selected[:insertion + 1] + "".join(helpers) + selected[insertion + 1:start] + fn + selected[end:]
            cells["helper0-%d-helper1-%d" % (kind0, kind1)] = text
    assert len(cells) == len(set(cells.values())) == 5
    return cells


if __name__ == "__main__":
    assert "--domain" in sys.argv
    d.REV = "d9995470"
    d.OUT_NAME = "recovery-fixedrc-pair-origins"
    d.SOURCE_PATHS = ("src/core/FixedRC.c",)
    d.DUMP_FLAGS = ("-v", "-save-temps", "-da")
    d.variants = variants
    d.main()
