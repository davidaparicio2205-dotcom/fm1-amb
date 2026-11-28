#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 David (davidaparicio2205-dotcom)
"""The emulator gate's frames (tools/emulator_check.sh OUTDIR) as PNGs and sheets: the LCD cut out of each panel
picture, the end of each piece (the Setup page: audio load, xruns, frames a second), the modes run as a sheet and a
GIF, the boot as a GIF, save and restore side by side.

  tools/emulator_frames.py OUTDIR DEST        (needs Pillow)
"""
import sys
from pathlib import Path

from PIL import Image

LCD = (370, 42, 850, 522)                         # the screen in fm1-live's 1280 x 820 panel picture (2x)


def lcd(p):
    im = Image.open(p).convert("RGB")
    if im.size == (1280, 820):
        im = im.crop(LCD).resize((240, 240), Image.LANCZOS)
    return im


def sheet(ims, cols, pad=6):
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    s = Image.new("RGB", (cols * (w + pad) - pad, rows * (h + pad) - pad))
    for i, im in enumerate(ims):
        s.paste(im, ((i % cols) * (w + pad), (i // cols) * (h + pad)))
    return s


def main():
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    dst.mkdir(parents=True, exist_ok=True)
    ends = []
    for p in range(8):
        d = src / f"piece{p}"
        if (d / "end.ppm").exists():
            im = lcd(d / "end.ppm")
            im.save(dst / f"piece{p + 1}-setup-end.png")
            ends.append(im)
    if ends:
        sheet(ends, 4).save(dst / "pieces-setup-end.png")
    modes = sorted((src / "modes" / "frames").glob("*.ppm"))
    if modes:
        ims = [lcd(f) for f in modes]
        sheet(ims[::max(1, len(ims) // 24)][:24], 6).save(dst / "modes-and-pages.png")
        ims[0].save(dst / "modes-and-pages.gif", save_all=True, append_images=ims[1:], duration=120, loop=0)
    boot = sorted((src / "boot" / "frames").glob("*.ppm"))
    if boot:
        ims = [lcd(f) for f in boot]
        ims[0].save(dst / "boot.gif", save_all=True, append_images=ims[1:], duration=100, loop=0)
        sheet(ims[::5][:8], 4).save(dst / "boot.png")
    pair = [lcd(src / s / "end.ppm") for s in ("save1", "save2") if (src / s / "end.ppm").exists()]
    if len(pair) == 2:
        sheet(pair, 2).save(dst / "save-restore.png")
    print(f"frames in {dst}")


if __name__ == "__main__":
    main()
