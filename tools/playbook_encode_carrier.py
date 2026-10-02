#!/usr/bin/env python3
"""Six complete-TU byte-carrier/counter-control encoding experiments."""
import playbook_small_patterns as driver


def variants(path, source):
    old_update = "\tiEncodeOffset = (iEncodeOffset == ENCODE_KEY_LEN - 1)\n\t\t\t? 0 : iEncodeOffset + 1;"
    branch = "\tif (iEncodeOffset == ENCODE_KEY_LEN - 1)\n\t\tiEncodeOffset = 0;\n\telse\n\t\tiEncodeOffset++;"
    assert source.count(old_update) == 1
    parents = {'int-helper': source}
    old_helper = "encode_step(int v)\n{\n\tchar c = (char)(v + offsetarr[iEncodeOffset] + '0');"
    byte_helper = "encode_step(unsigned char c)\n{\n\tc += offsetarr[iEncodeOffset] + '0';"
    assert source.count(old_helper) == 1
    parents['byte-helper'] = source.replace(old_helper, byte_helper)
    start, end, fn = driver.function(source, 'cEncodeChar')
    assert fn.count('\treturn encode_step((int)c);') == 1
    fn = fn.replace('\treturn encode_step((int)c);', "\tc += offsetarr[iEncodeOffset] + '0';\n\n" + old_update + '\n\treturn (char)c;')
    parents['direct-byte'] = source[:start] + fn + source[end:]
    cells = {}
    for label, text in parents.items():
        cells[label + '-ternary'] = text
        if label == 'direct-byte':
            start, end, fn = driver.function(text, 'cEncodeChar')
            assert fn.count(old_update) == 1
            fn = fn.replace(old_update, branch)
            text = text[:start] + fn + text[end:]
        else:
            assert text.count(old_update) == 1
            text = text.replace(old_update, branch)
        cells[label + '-branch'] = text
    assert len(cells) == len(set(cells.values())) == 6
    # The shared driver expects its unchanged control to be called baseline.
    cells['baseline'] = cells.pop('int-helper-ternary')
    return {'baseline': cells.pop('baseline'), **cells}


if __name__ == '__main__':
    driver.REV = '1dc5210d'
    driver.OUT_NAME = 'playbook-encode-carrier'
    driver.SOURCE_PATHS = ('src/core/encode.c',)
    driver.variants = variants
    driver.main()
