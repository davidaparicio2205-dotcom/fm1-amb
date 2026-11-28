#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 David (davidaparicio2205-dotcom)
"""The eras' screens captured from the host simulator (the firmware's own UI code, app/build/host/halflight_host):
for each piece, a few seconds at 30 frames a second after it has grown for a while, as a GIF, a still and a contact
sheet of every era; plus the intro, a change of era and every page over a scene. Into docs/visuals/ (or OUTDIR).

  tools/capture_visuals.py [OUTDIR]          (needs Pillow; ./build.sh --host first)
"""
import subprocess
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
HOST = ROOT / "app/build/host/halflight_host"
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs/visuals"
WORK = ROOT / "app/build/capture"
ERAS = ["after-dark", "long-glow", "clean-signal", "cut-and-paste"]
NAMES = ["Monochrome", "Night Bus", "Pale Piano", "Neon Fog", "Glass Choir", "Glitch Garden", "Fragments", "Static Folk"]
FRAME_MS = 33
FRAMES = 90                                       # 3 s


def run(script, name):
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / f"{name}.amb").write_text(script)
    subprocess.run([str(HOST), str(WORK / f"{name}.amb"), str(WORK)], check=True, stdout=subprocess.DEVNULL)


def gif(frames, path, scale=1):
    ims = [Image.open(f).convert("RGB") for f in frames]
    if scale != 1:
        ims = [i.resize((i.width * scale, i.height * scale), Image.NEAREST) for i in ims]
    ims[0].save(path, save_all=True, append_images=ims[1:], duration=FRAME_MS, loop=0, optimize=True)


def sheet(files, path, cols, scale=1, pad=8, bg=(0, 0, 0)):
    ims = [Image.open(f).convert("RGB") for f in files]
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    s = Image.new("RGB", (cols * (w + pad) - pad, rows * (h + pad) - pad), bg)
    for i, im in enumerate(ims):
        s.paste(im, ((i % cols) * (w + pad), (i // cols) * (h + pad)))
    if scale != 1:
        s = s.resize((s.width * scale, s.height * scale), Image.NEAREST)
    s.save(path)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # every piece: grown for 20 s, then 3 s of frames
    script = ["wait 2000"]
    for p in range(8):
        if p:
            script.append("turn PRESET 1")
        script.append("wait 20000")
        for f in range(FRAMES):
            script += [f"shot p{p}_{f:03d}.png", f"wait {FRAME_MS}"]
    run("\n".join(script) + "\n", "pieces")
    stills = []
    for p in range(8):
        frames = [WORK / f"p{p}_{f:03d}.png" for f in range(FRAMES)]
        slug = NAMES[p].lower().replace(" ", "-")
        gif(frames, OUT / f"piece{p + 1}-{slug}.gif")
        still = OUT / f"piece{p + 1}-{slug}.png"
        Image.open(frames[45]).save(still)
        stills.append(still)
        if p % 2 == 0:
            Image.open(frames[45]).save(OUT / f"era{p // 2 + 1}-{ERAS[p // 2]}.png")
    sheet(stills, OUT / "contact-sheet.png", 4)
    # the intro, and a change of era (the dissolve), frame by frame
    script = []
    for f in range(60):
        script += [f"shot intro_{f:03d}.png", f"wait {FRAME_MS}"]
    script += ["wait 8000"]
    for e in range(3):
        script.append("turn PRESET 2")
        for f in range(36):
            script += [f"shot change{e}_{f:03d}.png", f"wait {FRAME_MS}"]
        script.append("wait 6000")
    run("\n".join(script) + "\n", "intro")
    gif([WORK / f"intro_{f:03d}.png" for f in range(60)], OUT / "intro.gif")
    gif([WORK / f"change{e}_{f:03d}.png" for e in range(3) for f in range(36)], OUT / "era-changes.gif")
    sheet([WORK / f"change0_{f:03d}.png" for f in range(0, 36, 6)], OUT / "era-change-frames.png", 6)
    # every page over each era's scene (legibility)
    script = ["wait 2000"]
    pages = ["HOME", "ENV", "ARP", "LFO", "SEQ", "FX", "GLO", "SEL"]
    for e in range(4):
        if e:
            script.append("turn PRESET 2")
        script.append("wait 12000")
        for k, b in enumerate(pages):
            script += [f"tap {b}", "wait 400", f"shot page{e}_{k}.png"]
    run("\n".join(script) + "\n", "pages")
    for e in range(4):
        sheet([WORK / f"page{e}_{k}.png" for k in range(8)], OUT / f"pages-era{e + 1}-{ERAS[e]}.png", 4)
    print(f"visuals in {OUT}")


if __name__ == "__main__":
    main()
