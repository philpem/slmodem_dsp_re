#!/usr/bin/env python3
"""Fetch pinned official GCC3 scheduler, dependency and alias sources."""
import batch_scheduler_source_fetch as prior


if __name__ == '__main__':
    prior.HASHES.update({
        'sched-deps.c': '8363efa4d797a2850367a59df6e9770869752315877816f0a7395d0e1a55fcd1',
        'alias.c': '61a92549e8d56c16385f1f4feb87feb5a391fc6ca6cc16016bd1838951b9c438',
    })
    prior.main()
