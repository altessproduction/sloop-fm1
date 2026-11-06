#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Adapt the upstream SLOOP WebAssembly HAL to the ROOTS DUB firmware tree."""
from pathlib import Path
import sys

p = Path(sys.argv[1])
s = p.read_text()

needle = '#include "core.h"\n'
insert = needle + '''
/* ROOTS DUB has the FM-1 second-core scheduler in the native build. A browser
 * AudioWorklet is single-threaded: keep the exact same DSP path, but run every
 * render unit on the worklet thread instead of dispatching to core 1. */
#include "cpu1.h"
static int cpu1_on(void) { return 0; }
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { (void)fn; (void)arg; return 0; }
static int cpu1_wait(void) { return 0; }
static uint32_t cpu1_ticks(void) { return fm1_ticks(); }
'''
if needle not in s:
    raise SystemExit("could not locate core.h insertion point")
if 'ROOTS DUB has the FM-1 second-core scheduler' not in s:
    s = s.replace(needle, insert, 1)

needle = '#include "ui_draw.c"\n'
if needle not in s:
    raise SystemExit("could not locate ui_draw.c include")
if '#include "ui_vis.c"' not in s:
    s = s.replace(needle, needle + '#include "ui_vis.c"\n', 1)

needle = '#include "storage.c"\n'
if needle not in s:
    raise SystemExit("could not locate storage.c include")
if '#include "scratch.h"' not in s:
    s = s.replace(needle, '#include "scratch.h"\n' + needle, 1)

p.write_text(s)
