#!/bin/sh
# Build the actual ROOTS DUB firmware sources as a browser AudioWorklet.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
EMU="$ROOT/build/sloop-web-emu"
PIN="bbfd8e5b3bfc452520f61afb0d94e2fdfe93fd9d"

rm -rf "$EMU"
git clone --quiet https://github.com/sabliran/sloop-web-emu.git "$EMU"
git -C "$EMU" checkout --quiet "$PIN"

GEN="$ROOT/build/gen"
mkdir -p "$GEN"
python3 "$ROOT/tools/gen_font.py" "$GEN/felucca_font.h"
python3 "$ROOT/tools/gen_icons.py" "$GEN/felucca_icons.h"
python3 "$ROOT/tools/gen_tables.py" "$GEN/felucca_tables.h"
python3 "$ROOT/tools/gen_samples.py" "$GEN/felucca_samples.h"
python3 "$ROOT/tools/gen_drumkits.py" "$GEN/felucca_drumkits.h"
python3 "$ROOT/tools/gen_fm6_patches.py" "$GEN/felucca_fm6.h"
python3 "$ROOT/tools/gen_logo.py" "$GEN/sloop_logo.h"

python3 "$ROOT/tools/patch_sloop_wasm.py" "$EMU/src/sloop_wasm.c"
(
  cd "$EMU"
  SLOOP="$ROOT" FELUCCA="$ROOT/build/no-felucca" sh ./build.sh
)
