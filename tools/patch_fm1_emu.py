#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Patch the MIT-licensed fm1-emu web frontend to load a same-origin .fwsc by URL."""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_fm1_emu.py PATH_TO_FM1_EMU_WEB_MAIN_JS")

path = Path(sys.argv[1])
source = path.read_text(encoding="utf-8")
needle = 'openInput.addEventListener("change", () => { openLocal(openInput.files[0]); openInput.value = ""; });'
addition = """// SLOOP ROOTS DUB integration: load our freshly built package from this site's own origin.
const packageUrl = params.get("package");
if (packageUrl) {
  (async () => {
    try {
      const url = new URL(packageUrl, location.href);
      if (url.origin !== location.origin) throw new Error("firmware package must be hosted on this site");
      const response = await fetch(url.href, { credentials: "same-origin", redirect: "error" });
      if (!response.ok) throw new Error(`HTTP ${response.status} while loading firmware package`);
      const bytes = await response.arrayBuffer();
      await openLocal(new File([bytes], "SLOOP-ROOTS-DUB.fwsc", { type: "application/octet-stream" }));
      status.textContent = "SLOOP ROOTS DUB package loaded · click Start";
    } catch (error) {
      status.textContent = `Firmware package error: ${error.message}`;
    }
  })();
}
"""
if source.count(needle) != 1:
    raise SystemExit("upstream fm1-emu changed: expected openInput handler exactly once")
path.write_text(source.replace(needle, addition + needle), encoding="utf-8")
print(f"Patched {path}")
