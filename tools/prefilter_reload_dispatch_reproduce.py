#!/usr/bin/env python3
"""Cross original callback reload ages with witnessed integer dispatch trees."""
import playbook_small_patterns as d


def change(function, old, new):
    assert function.count(old) == 1, old
    return function.replace(old, new)


def variants(path, source):
    start, end, body = d.function(source, 'V90PreFilter::selectFilter')
    cells = {}
    for reload in (False, True):
        for early in (False, True):
            for switches in (False, True):
                function = body
                if reload:
                    function = change(function, '\t\tp = params;\n\t\tif (wide)', '\t\tif (wide)')
                    for message in ('(4.2kHz Null)', '(4kHz Null)', 'Force coeff type array 1',
                                    'Force coeff type array 2', 'Force coeff type array 3',
                                    'according to codec type'):
                        pos = function.index(message)
                        close = function.index(';', pos)+1
                        function = function[:close]+'\n\t\tp = params;'+function[close:]
                if early:
                    for number in (1, 2):
                        beginning = function.index('if (force == '+str(number)+')')
                        call = function.index('\n\t\t\tedprintf', beginning)
                        assignment = function.index('\n\t\t\ttype = '+str(number)+';', call)
                        function = function[:assignment]+function[assignment:].replace('\n\t\t\ttype = '+str(number)+';', '', 1)
                        function = function[:call]+'\n\t\t\ttype = '+str(number)+';'+function[call:]
                if switches:
                    beginning = function.index('\t\tif (force == 1)')
                    finish = function.index('\n\t\tif (wide)', beginning) if reload else function.index('\n\t\tp = params;', beginning)
                    old = function[beginning:finish]
                    arms = []
                    for number, label in ((1, '1'), (2, '2'), (3, '3'), (-1, '-1')):
                        marker = 'if (force == '+str(number)+') {'
                        a = old.index(marker)+len(marker)
                        b = old.index('\n\t\t}', a)
                        arms.append('\t\tcase '+label+':'+old[a:b]+'\n\t\t\tbreak;')
                    a = old.index('else {', old.index('if (force == -1)'))+len('else {')
                    b = old.index('\n\t\t}', a)
                    arms.append('\t\tdefault:'+old[a:b]+'\n\t\t\tbreak;')
                    function = function[:beginning]+'\t\tswitch (force) {\n'+'\n'.join(arms)+'\n\t\t}\n'+function[finish:]
                    old = '\t\t\tif (t == 3) {\n\t\t\t\tlen = 40;\n\t\t\t} else if (t != 2 && t != 1) {\n\t\t\t\tedprintf(BUGMSG);\n\t\t\t\tloop = refLoop;\n\t\t\t}'
                    new = '\t\t\tswitch (t) {\n\t\t\tcase 1:\n\t\t\tcase 2:\n\t\t\t\tbreak;\n\t\t\tcase 3:\n\t\t\t\tlen = 40;\n\t\t\t\tbreak;\n\t\t\tdefault:\n\t\t\t\tedprintf(BUGMSG);\n\t\t\t\tloop = refLoop;\n\t\t\t\tbreak;\n\t\t\t}'
                    function = change(function, old, new)
                    a = function.index('\t\t\tif (t == 2) {')
                    b = function.index('\n\t\t}\n\n\t\tsetCoefficients', a)
                    function = function[:a]+'''\t\t\tswitch (t) {
\t\t\tcase 2:
\t\t\t\tif ((unsigned int)want > 30)
\t\t\t\t\trow = 30;
\t\t\t\tcoef = bank2(row);
\t\t\t\tbreak;
\t\t\tcase 3:
\t\t\t\tif ((unsigned int)want > 50)
\t\t\t\t\trow = 50;
\t\t\t\tcoef = bank3(row);
\t\t\t\tbreak;
\t\t\tdefault:
\t\t\t\tif (t != 1)
\t\t\t\t\tedprintf(BUGMSG);
\t\t\tcase 1:
\t\t\t\tif ((unsigned int)want > 30)
\t\t\t\t\trow = 30;
\t\t\t\tcoef = bank1(row);
\t\t\t\tbreak;
\t\t\t}'''+function[b:]
                    a = function.index('\tif (type == 2) {')
                    b = function.index('\n\n\tedprintf("V90PreFilter: Filter Gain', a)
                    function = function[:a]+'''\tswitch (type) {
\tcase 2:
\t\tcoef = bank2(g);
\t\tsetCoefficients(coef, 20);
\t\tbreak;
\tcase 3:
\t\tcoef = bank3(g);
\t\tsetCoefficients(coef, 40);
\t\tgain = g - 20;
\t\tbreak;
\tdefault:
\t\tcoef = bank1(g);
\t\tsetCoefficients(coef, 20);
\t\tbreak;
\t}'''+function[b:]
                label = 'baseline' if not any((reload, early, switches)) else f'reload{int(reload)}-early{int(early)}-switch{int(switches)}'
                cells[label] = source[:start]+function+source[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    d.REV = '9025b8d8'
    d.SOURCE_PATHS = ('src/pump/v90/V90PreFilter.cpp',)
    d.OUT_NAME = 'prefilter-reload-dispatch'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
