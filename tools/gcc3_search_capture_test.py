#!/usr/bin/env python3
"""Argument and compiler-flag tests for the read-only GCC capture tool."""
from pathlib import Path
import sys

# tools/dis.py must not shadow the standard-library dis imported by unittest.
_TOOLS = str(Path(__file__).resolve().parent)
sys.path[:] = [entry for entry in sys.path if str(Path(entry or '.').resolve()) != _TOOLS]
import unittest

sys.path.insert(0, _TOOLS)
import gcc3_search_capture as capture


class CaptureFlagsTest(unittest.TestCase):
    def test_rtl_dump_option_defaults_off_and_is_explicitly_enabled(self):
        required = ['--source', 'unit.c', '--output', 'build/capture']
        self.assertFalse(capture.build_parser().parse_args(required).rtl_dumps)
        self.assertTrue(capture.build_parser().parse_args(required + ['--rtl-dumps']).rtl_dumps)

    def test_default_capture_flags_omit_da_and_keep_observation_flags(self):
        flags = capture.build_capture_flags({'flags': '-O3 -fno-strict-aliasing'},
                                            Path('unit.c'), 'cc1')
        self.assertIn('-v', flags)
        self.assertIn('-save-temps', flags)
        self.assertNotIn('-da', flags)
        self.assertNotIn('-DDSPLIB_REPRODUCE_BUGS', flags)
        effective = capture.tc.add_reproduce_bugs(flags)
        self.assertEqual(effective.count('-DDSPLIB_REPRODUCE_BUGS'), 1)
        command = capture.tc.compile_shell('/toolchain', flags, 'out.o', 'unit.c')
        self.assertEqual(command.count('-DDSPLIB_REPRODUCE_BUGS'), 1)

    def test_rtl_dump_flag_is_opt_in_after_cxx_and_dcr_overrides(self):
        config = {'flags': '-O3', 'cxx': '-fno-exceptions', 'dcr': '-fno-inline'}
        plain = capture.build_capture_flags(config, Path('dcr.c'), 'cc1plus')
        dumped = capture.build_capture_flags(config, Path('dcr.c'), 'cc1plus', rtl_dumps=True)
        self.assertNotIn('-da', plain)
        self.assertEqual(dumped.count('-da'), 1)
        self.assertLess(dumped.index('-fno-exceptions'), dumped.index('-da'))
        self.assertLess(dumped.index('-fno-inline'), dumped.index('-da'))


if __name__ == '__main__':
    unittest.main()
