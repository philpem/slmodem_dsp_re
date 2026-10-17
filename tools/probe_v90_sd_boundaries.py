#!/usr/bin/env python3
"""Replay 64 constructor-created SD threshold/limit histories on Gentoo.

First run make period T=t_v90spectral. This component-only probe is outside
default discovery; it supplies quiet NaNs externally, never plants history.
Compile/link/run failures remain nonzero. Existing period objects must be fresh.
"""
from probe_v90_connection_boundaries import main


if __name__ == '__main__':
    raise SystemExit(main(
        fixture='test/explore/t_v90_sd_boundaries.cpp',
        source='src/pump/v90/V90SdDetector.cpp',
        related='test/unit/t_v90spectral.cpp',
        boundary='64 constructors: each threshold finite/quiet-NaN, limits0/1/3/0x80000000; public process/reset histories; no modem reachability claim',
        out_name='v90-sd-boundaries-probe'))
