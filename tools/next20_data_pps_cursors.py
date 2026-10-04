#!/usr/bin/env python3
"""Original V22 pulse-shaper coefficient walks and history-window ownership."""
import playbook_small_patterns as d

def variants(path, source):
    a, z, fn = d.function(source, 'V22_PPS_filter')
    startup = ('\t\t\tfor (k = 0; k < V22_PPS_TAPS; k++) {\n'
               '\t\t\t\tacci += hi[k] * ci[phase + k];\n'
               '\t\t\t\taccq += hq[k] * cq[phase + k];\n\t\t\t}')
    steady = ('\t\t\tfor (k = 0; k < V22_PPS_TAPS; k++) {\n'
              '\t\t\t\tacci += hi[widx - (V22_PPS_TAPS - 1) + k] *\n'
              '\t\t\t\t\tci[phase * hlen + k];\n'
              '\t\t\t\taccq += hq[widx - (V22_PPS_TAPS - 1) + k] *\n'
              '\t\t\t\t\tcq[phase * hlen + k];\n\t\t\t}')
    assert fn.count(startup) == fn.count(steady) == 1
    cells = {'baseline': source}
    for walk, window in ((1, 0), (0, 1), (1, 1)):
        x = fn
        st = startup
        ss = steady
        if walk:
            st = ('\t\t\tconst short *pci = ci + phase;\n'
                  '\t\t\tconst short *pcq = cq + phase;\n\n' + st)
            ss = ('\t\t\tconst short *pci = ci + phase * hlen;\n'
                  '\t\t\tconst short *pcq = cq + phase * hlen;\n\n' + ss)
            st = st.replace('ci[phase + k]', '*pci++').replace('cq[phase + k]', '*pcq++')
            ss = ss.replace('ci[phase * hlen + k]', '*pci++').replace('cq[phase * hlen + k]', '*pcq++')
        if window:
            ss = ('\t\t\tconst short *shi = hi + widx - (V22_PPS_TAPS - 1);\n'
                  '\t\t\tconst short *shq = hq + widx - (V22_PPS_TAPS - 1);\n\n' + ss)
            ss = ss.replace('hi[widx - (V22_PPS_TAPS - 1) + k]', 'shi[k]')
            ss = ss.replace('hq[widx - (V22_PPS_TAPS - 1) + k]', 'shq[k]')
        x = x.replace(startup, st).replace(steady, ss)
        label = ('coefficient-walk-' if walk else '') + ('history-window' if window else 'history-indexed')
        cells[label] = source[:a] + x + source[z:]
    return cells

if __name__ == '__main__':
    d.REV = '8af3af53'
    d.SOURCE_PATHS = ('src/pump/v22/v22_pps.c',)
    d.OUT_NAME = 'next20-data-pps-cursors'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
