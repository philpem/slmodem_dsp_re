#!/usr/bin/env python3
"""Complete-TU readonly initializer visibility crossed with expansion regime."""
import playbook_small_patterns as driver


def variants(path, source):
    declaration = 'static const int block_size_table[12]'
    begin = source.index(declaration)
    end = source.index('\n};', begin) + len('\n};')
    table = source[begin:end]
    deferred = source[:begin] + declaration + ';' + source[end:]
    _, finish, _ = driver.function(deferred, 'BwChDem_Create')
    deferred = deferred[:finish] + '\n\n' + table + deferred[finish:]
    return {'baseline':source, 'deferred':deferred,
            'no-unit':source, 'deferred-no-unit':deferred}


original_compile = driver.tc.compile_shell


def compile_cell(compiler_path, flags, output, source):
    if '/no-unit/' in output or '/deferred-no-unit/' in output:
        flags = flags + ['-fno-unit-at-a-time']
    return original_compile(compiler_path, flags, output, source)


if __name__ == '__main__':
    driver.REV = '3f4d11d4'
    driver.OUT_NAME = 'playbook-bwch-unit-visibility'
    driver.SOURCE_PATHS = ('src/pump/v23/bwchdem.c',)
    driver.variants = variants
    driver.tc.compile_shell = compile_cell
    driver.main()
