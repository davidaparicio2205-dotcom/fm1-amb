#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 David (davidaparicio2205-dotcom)
# The emulator gate (docs/ARCHITECTURE.md, the emulator): a built package in fm1-emu's headless fm1-live at 120 MHz
# (half the FM-1's clock), nothing near a device.
#   tools/emulator_check.sh PACKAGE.fwsc FM1_LIVE OUTDIR [MINUTES]
#   1. boot: the splash and the intro (frames), the identity in the first screen
#   2. every piece for MINUTES (default 5) after it is picked, the Setup page up: the panel at the end (the Setup
#      page's foot shows the audio load now and at its worst since the boot, the xruns and the frame rate), and the
#      audio frames played (FM1_BENCH_JSON: a gap would show as frames missing against the time)
#   3. every ALGORITHM mode and every page on every era (frames), no halt
#   4. a save: a piece picked and a setting turned, SAVE, then a new run on the same flash: the piece comes back
# Results: OUTDIR/results.txt and the frames (PPM; tools/emulator_frames.py makes PNGs and sheets).
set -eu
PKG="$1"
LIVE="$2"
OUT="$3"
MIN="${4:-5}"
MS=$((MIN * 60000))
mkdir -p "$OUT"
RES="$OUT/results.txt"
: > "$RES"
run() {   # run NAME MS KEYS FRAME_MS: --bench at 120 MHz with KEYS, the panel at the end; FRAME_MS > 0: the panel every
          # FRAME_MS too (3 MB a frame: keep them few)
    name="$1"; ms="$2"; keys="$3"; fms="$4"
    mkdir -p "$OUT/$name"
    if [ "$fms" -gt 0 ]; then
        env FM1_BENCH_KEYS="$keys" FM1_BENCH_JSON="$OUT/$name/bench.json" FM1_BENCH_PANEL="$OUT/$name/end.ppm" \
            FM1_BENCH_FRAMES="$OUT/$name/frames,$fms" \
            "$LIVE" --fwsc "$PKG" --mhz 120 --bench "$ms" > "$OUT/$name/log.txt" 2>&1 || echo "$name: fm1-live exit $?" >> "$RES"
        rm -f "$OUT/$name/frames/audio.raw"
    else
        env FM1_BENCH_KEYS="$keys" FM1_BENCH_JSON="$OUT/$name/bench.json" FM1_BENCH_PANEL="$OUT/$name/end.ppm" \
            "$LIVE" --fwsc "$PKG" --mhz 120 --bench "$ms" > "$OUT/$name/log.txt" 2>&1 || echo "$name: fm1-live exit $?" >> "$RES"
    fi
    echo "$name: $(tr -d '\n' < "$OUT/$name/bench.json" 2>/dev/null || echo 'no bench.json')" >> "$RES"
}
# 1. the boot, frame by frame (every 100 ms for 4 s)
run boot 4000 "3900:HOME,4050:HOME^" 200
# 2. every piece, MIN minutes each, on the Setup page
p=0
while [ $p -lt 8 ]; do
    keys=""
    i=0
    t=2500
    while [ $i -lt $p ]; do keys="$keys$t:e6:+1,"; t=$((t + 300)); i=$((i + 1)); done
    keys="${keys}$((t + 500)):GLO,$((t + 650)):GLO^"
    run "piece$p" $((t + 1000 + MS)) "$keys" 0
    p=$((p + 1))
done
# 3. the modes (ALGORITHM, eight steps) and the pages, on each era
keys=""
t=3000
e=0
while [ $e -lt 4 ]; do
    for b in HOME ENV ARP LFO SEQ FX GLO SCL; do keys="$keys$t:$b,$((t + 150)):$b^,"; t=$((t + 700)); done
    m=0
    while [ $m -lt 8 ]; do keys="$keys$t:e1:+1,"; t=$((t + 600)); m=$((m + 1)); done
    keys="$keys$t:e6:+1,$((t + 300)):e6:+1,"
    t=$((t + 4000))
    e=$((e + 1))
done
run modes "$t" "${keys%,}" 700
# 4. save and restore: Neon Fog (4th), the Bells page's level up 3, SAVE; then the same flash again
rm -f "$OUT/flash.bin"
mkdir -p "$OUT/save1" "$OUT/save2"
env FM1_BENCH_KEYS="2500:e6:+1,2800:e6:+1,3100:e6:+1,3600:ARP,3750:ARP^,4000:e2:+3,5000:SAVE,5150:SAVE^" \
    FM1_BENCH_PANEL="$OUT/save1/end.ppm" "$LIVE" --fwsc "$PKG" --mhz 120 --flash "$OUT/flash.bin" --bench 9000 \
    > "$OUT/save1/log.txt" 2>&1 || echo "save1: exit $?" >> "$RES"
env FM1_BENCH_KEYS="2500:ARP,2650:ARP^" FM1_BENCH_PANEL="$OUT/save2/end.ppm" \
    "$LIVE" --fwsc "$PKG" --mhz 120 --flash "$OUT/flash.bin" --bench 4000 > "$OUT/save2/log.txt" 2>&1 \
    || echo "save2: exit $?" >> "$RES"
echo "save/restore: see save1/end.ppm (Neon Fog, Bells level 37) and save2/end.ppm (after a new boot)" >> "$RES"
cat "$RES"
