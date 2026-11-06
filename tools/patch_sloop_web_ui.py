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

# Add direct ROOTS DUB controls to the emulator shell. These controls call the
# same firmware project state used by fx.c; they do not create a Web Audio path.
s = js.read_text()
anchor = "function sendMaster() { if (node) node.port.postMessage({ t: 'adc', ch: 4, v: masterVal }); }"
insert = anchor + r"""

const rootsState = { enabled: false, gains: [127, 127, 127, 127] };
let rootsUi = null;
function sendRoots() {
    if (node) node.port.postMessage({ t: 'roots', enabled: rootsState.enabled, gains: rootsState.gains.slice() });
}
function setRootsControls(enabled, gains) {
    rootsState.enabled = !!enabled;
    rootsState.gains = gains.slice(0, 4).map(v => Math.max(0, Math.min(127, v | 0)));
    if (!rootsUi) return;
    rootsUi.toggle.checked = rootsState.enabled;
    rootsUi.gains.forEach((g, i) => { g.value = rootsState.gains[i]; rootsUi.values[i].textContent = rootsState.gains[i]; });
}
"""
if anchor not in s:
    raise SystemExit("could not locate master control function")
if "const rootsState =" not in s:
    s = s.replace(anchor, insert, 1)

panel_anchor = "/* --------------------------------------------------- computer keyboard --- */"
panel = r"""
function buildRootsPanel() {
    const panel = document.createElement('section');
    panel.id = 'roots-controls';
    panel.style.cssText = 'width:min(963px,96vw);margin:22px auto 26px;padding:18px 20px;background:#181b20;border:1px solid #30353d;border-radius:12px;color:#d5dae2;font:13px Inter Tight,system-ui,sans-serif;box-sizing:border-box;';
    const heading = document.createElement('div');
    heading.style.cssText = 'display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:14px;flex-wrap:wrap;';
    heading.innerHTML = '<strong style="letter-spacing:2px;font-size:12px">ROOTS DUB · MASTER ISOLATOR</strong><span style="color:#858d98;font-size:11px">FIRMWARE DSP · SUB / BASS / MID / HIGH</span>';
    panel.appendChild(heading);
    const toggleLabel = document.createElement('label');
    toggleLabel.style.cssText = 'display:flex;align-items:center;gap:9px;margin:0 0 14px;font-weight:600;';
    const toggle = document.createElement('input');
    toggle.type = 'checkbox';
    toggle.addEventListener('change', () => { rootsState.enabled = toggle.checked; sendRoots(); });
    toggleLabel.append(toggle, document.createTextNode('ACTIVER L’ISOLATEUR'));
    panel.appendChild(toggleLabel);
    const grid = document.createElement('div');
    grid.style.cssText = 'display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:12px 18px;';
    const labels = ['SUB', 'BASS', 'MID', 'HIGH'];
    const gains = [], values = [];
    labels.forEach((name, i) => {
        const wrap = document.createElement('label');
        wrap.style.cssText = 'display:grid;grid-template-columns:1fr auto;gap:6px 10px;align-items:center;color:#aeb6c2;font-size:11px;letter-spacing:1px;';
        const title = document.createElement('span'); title.textContent = name;
        const value = document.createElement('output'); value.textContent = '127'; value.style.cssText = 'color:#e8b84b;font-variant-numeric:tabular-nums;';
        const range = document.createElement('input');
        range.type = 'range'; range.min = '0'; range.max = '127'; range.step = '1'; range.value = '127';
        range.style.cssText = 'grid-column:1 / -1;width:100%;accent-color:#e8b84b;';
        range.addEventListener('input', () => { rootsState.gains[i] = +range.value; value.textContent = range.value; sendRoots(); });
        wrap.append(title, value, range); grid.appendChild(wrap);
        gains.push(range); values.push(value);
    });
    panel.appendChild(grid);
    const note = document.createElement('p');
    note.textContent = 'Commandes reliées à l’isolateur quatre bandes du bus master du firmware. Les réglages sont conservés avec le projet lors de son enregistrement.';
    note.style.cssText = 'margin-top:13px;color:#7f8996;font-size:11px;line-height:1.5;';
    panel.appendChild(note);
    const play = document.createElement('p');
    play.textContent = 'SKANK À UNE TOUCHE : régler CHORD = SKANK sur la piste, puis jouer une seule note. SIREN DUB : choisir le moteur SIREN et l’un de ses presets ; les pages moteur règlent TUNE, SWEEP, TIME, LFO, DEPTH, CUT, RES et WAVE. L’écho utilise le send DLY de la piste. Presets ROOTS DUB : DUB SUB, ROOTS BASS, SKANK STAB, DUB LEAD, DUB CHORD, SKANK KEYS et ORGAN BUBBLE.';
    play.style.cssText = 'margin-top:9px;color:#aeb6c2;font-size:11px;line-height:1.6;';
    panel.appendChild(play);
    device.after(panel);
    rootsUi = { toggle, gains, values };
    setRootsControls(rootsState.enabled, rootsState.gains);
}

"""
if panel_anchor not in s:
    raise SystemExit("could not locate keyboard insertion point")
if "function buildRootsPanel()" not in s:
    s = s.replace(panel_anchor, panel + panel_anchor, 1)

old_msg = "        else if (m.t === 'flash') saveFlash(m.data);"
new_msg = old_msg + "\n        else if (m.t === 'roots-state') setRootsControls(m.enabled, m.gains);"
if old_msg not in s:
    raise SystemExit("could not locate flash message handler")
if "roots-state" not in s:
    s = s.replace(old_msg, new_msg, 1)

old_conn = "    node.connect(ctx.destination);\n    await ctx.resume();\n    sendMaster();"
new_conn = "    node.connect(ctx.destination);\n    node.port.postMessage({ t: 'roots-get' });\n    await ctx.resume();\n    sendMaster();"
if old_conn not in s:
    raise SystemExit("could not locate audio node connection")
if "t: 'roots-get'" not in s:
    s = s.replace(old_conn, new_conn, 1)

old_calls = "buildKnobs();\nbuildButtons();\nbuildKeyboard();\nconst screenCanvas = buildScreen();"
new_calls = "buildKnobs();\nbuildButtons();\nbuildKeyboard();\nbuildRootsPanel();\nconst screenCanvas = buildScreen();"
if old_calls not in s:
    raise SystemExit("could not locate UI build calls")
if "buildRootsPanel();" not in s:
    s = s.replace(old_calls, new_calls, 1)
js.write_text(s)

worklet = web / "worklet.js"
w = worklet.read_text()
old = """            else if (m.t === 'adc') this.e.emu_adc_set(m.ch, m.v);
            else if (m.t === 'midi') this.e.emu_midi(m.p);"""
new = """            else if (m.t === 'adc') this.e.emu_adc_set(m.ch, m.v);
            else if (m.t === 'roots') this.e.emu_roots_set(m.enabled ? 1 : 0, m.gains[0], m.gains[1], m.gains[2], m.gains[3]);
            else if (m.t === 'roots-get') this.port.postMessage({ t: 'roots-state', enabled: !!this.e.emu_roots_get(0),
                gains: [1, 2, 3, 4].map(i => this.e.emu_roots_get(i)) });
            else if (m.t === 'midi') this.e.emu_midi(m.p);"""
if old not in w:
    raise SystemExit("could not locate worklet message handler")
if "m.t === 'roots-get'" not in w:
    w = w.replace(old, new, 1)
worklet.write_text(w)

index = web / "index.html"
h = index.read_text()
if '<title>SLOOP — FM-1 emulator</title>' not in h:
    raise SystemExit("could not locate HTML title")
h = h.replace('<title>SLOOP — FM-1 emulator</title>', '<title>SLOOP ROOTS DUB — FM-1 emulator</title>', 1)
h = h.replace('<meta name="apple-mobile-web-app-title" content="SLOOP">', '<meta name="apple-mobile-web-app-title" content="SLOOP ROOTS DUB">', 1)
index.write_text(h)
