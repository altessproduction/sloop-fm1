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
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { fn(arg); return 0; }
static int cpu1_wait(void) { return 0; }
static uint32_t cpu1_ticks(void) { return fm1_ticks(); }
'''
if needle not in s:
    raise SystemExit("could not locate core.h insertion point")
if 'ROOTS DUB has the FM-1 second-core scheduler' not in s:
    s = s.replace(needle, insert, 1)

# The browser input HAL has no MIDI-remote button matrix state.
needle = 'static uint8_t fm1_led[FM1_NCOL];\n'
if needle not in s:
    raise SystemExit("could not locate browser LED state")
if 'static volatile uint32_t fm1_remote_btn;' not in s:
    s = s.replace(needle, 'static volatile uint32_t fm1_remote_btn;\n' + needle, 1)

# Map the firmware's XIP and JEDEC checks onto the emulator's RAM flash image.
needle = '#include "storage.c"\n'
insert = '''#define ST_XIP_PTR(off) (emu_flash + ((off) - EMU_FLASH_BASE))
#define FL_JEDEC_OK(id) ((id) == 0x856014u || (id) == 0xB36014u)
''' + needle
if needle not in s:
    raise SystemExit("could not locate storage.c insertion point")
if 'ST_XIP_PTR(off)' not in s:
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


# Expose the firmware's persisted ROOTS master-isolator settings to the emulator UI.
# These call the same project.c state used by the audio mixer and project serializer.
needle = '#include "project.c"\n'
insert = needle + '''
WASM_EXPORT("emu_roots_set") void emu_roots_set(uint32_t enabled, uint32_t sub, uint32_t bass,
                                                uint32_t mid, uint32_t high)
{
    uint8_t gain[4];
    gain[0] = (uint8_t)clamp((int32_t)sub, 0, 127);
    gain[1] = (uint8_t)clamp((int32_t)bass, 0, 127);
    gain[2] = (uint8_t)clamp((int32_t)mid, 0, 127);
    gain[3] = (uint8_t)clamp((int32_t)high, 0, 127);
    fm1_irq_off();
    proj_roots_set((uint8_t)(enabled != 0u), gain);
    fm1_irq_on();
}
WASM_EXPORT("emu_roots_get") uint32_t emu_roots_get(uint32_t index)
{
    uint8_t enabled, gain[4];
    proj_roots_get(&enabled, gain);
    return index == 0u ? enabled : (index <= 4u ? gain[index - 1u] : 0u);
}
WASM_EXPORT("emu_chord_set") void emu_chord_set(uint32_t track, uint32_t mode)
{
    if (track < NPART) {
        fm1_irq_off();
        trk[track].p[P_CHORD] = (int16_t)clamp((int32_t)mode, 0, 9);
        fm1_irq_on();
    }
}
WASM_EXPORT("emu_chord_get") uint32_t emu_chord_get(uint32_t track)
{
    return track < NPART ? (uint32_t)clamp(trk[track].p[P_CHORD], 0, 9) : 0u;
}
'''
if needle not in s:
    raise SystemExit("could not locate project.c insertion point")
if 'WASM_EXPORT("emu_roots_set")' not in s:
    s = s.replace(needle, insert, 1)

p.write_text(s)
