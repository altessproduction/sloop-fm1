#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Brand the dedicated SLOOP web emulator build as ROOTS DUB and disable the Felucca switch."""
from pathlib import Path
import sys

web = Path(sys.argv[1])
js = web / "emu.js"
s = js.read_text()
replacements = [
    ("const FW = (localStorage.getItem('fw') === 'felucca') ? 'felucca' : 'sloop';",
     "const FW = 'sloop'; // This deployment contains the ROOTS DUB firmware only."),
    ("document.querySelectorAll('#fw span').forEach(s => {",
     "document.getElementById('fw').style.display = 'none'; // no Felucca binary in this build\ndocument.querySelectorAll('#fw span').forEach(s => {"),
    ("document.getElementById('title').innerHTML = FW === 'felucca'\n    ? '<b>FELUCCA</b> &middot; M-VAVE FM-1 &middot; browser emulator'\n    : '<b>SLOOP</b> 2.3 &middot; M-VAVE FM-1 &middot; browser emulator';",
     "document.getElementById('title').innerHTML = '<b>SLOOP ROOTS DUB</b> &middot; M-VAVE FM-1 &middot; browser emulator';"),
    ("document.title = (FW === 'felucca' ? 'Felucca' : 'SLOOP') + ' — FM-1 emulator';",
     "document.title = 'SLOOP ROOTS DUB — FM-1 emulator';"),
    ("    'runs the real ' + (FW === 'felucca' ? 'Felucca' : 'SLOOP') + ' firmware, compiled to WebAssembly · sound on';",
     "    'runs the real SLOOP ROOTS DUB firmware, compiled to WebAssembly · sound on';"),
]
for old, new in replacements:
    if old not in s:
        raise SystemExit("could not locate expected emulator UI text: " + old[:90])
    s = s.replace(old, new, 1)
js.write_text(s)

index = web / "index.html"
h = index.read_text()
if '<title>SLOOP — FM-1 emulator</title>' not in h:
    raise SystemExit("could not locate HTML title")
h = h.replace('<title>SLOOP — FM-1 emulator</title>', '<title>SLOOP ROOTS DUB — FM-1 emulator</title>', 1)
h = h.replace('<meta name="apple-mobile-web-app-title" content="SLOOP">', '<meta name="apple-mobile-web-app-title" content="SLOOP ROOTS DUB">', 1)
index.write_text(h)
