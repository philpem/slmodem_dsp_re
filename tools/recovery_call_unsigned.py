#!/usr/bin/env python3
"""Cross original-backed unsigned S-register consumers and queue offsets.

Use --historical-headers when replaying after the header recovery is adopted.
The fifth cell isolates the original S56 member-owner reload.
"""
import subprocess
import playbook_small_patterns as driver


def carriers(source):
    old = '\tint mode;'
    assert source.count(old) == 1
    source = source.replace(old, '\tunsigned int mode;')
    old = "buf[0] = modem_get_sreg(st->modem, 16) < 1 ? 'p' : 't';"
    assert source.count(old) == 1
    return source.replace(old, "buf[0] = (unsigned int)modem_get_sreg(st->modem, 16) < 1 ? 'p' : 't';")


def queue_header(source):
    for field in ('head','tail','active'):
        old = '\tint\t' + field + ';'
        assert source.count(old) == 1
        source = source.replace(old, '\tunsigned int\t' + field + ';')
    return source


def variants(path, source):
    unsigned = carriers(source)
    return {'baseline':source, 'unsigned-sregs':unsigned,
            'unsigned-queues':source, 'unsigned-both':unsigned,
            'unsigned-owner':unsigned.replace('mode = modem_get_sreg(modem, 56);',
                                              'mode = modem_get_sreg(st->modem, 56);')}


if __name__ == '__main__':
    driver.REV = '95e01c27'
    driver.SOURCE_PATHS = ('src/call/call.c',)
    driver.OUT_NAME = 'recovery-call-unsigned'
    driver.DUMP_FLAGS = ('-v','-save-temps','-da','-dP')
    header = subprocess.check_output(['git','show',driver.REV+':include/dsplib/call.h'],cwd=driver.ROOT,text=True)
    driver.HEADER_OVERLAYS = lambda path,label: {'dsplib/call.h':queue_header(header)} if label in ('unsigned-queues','unsigned-both','unsigned-owner') else {}
    driver.variants = variants
    driver.main()
