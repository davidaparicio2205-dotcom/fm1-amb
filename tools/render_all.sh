#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 David (davidaparicio2205-dotcom)
# Every piece through the offline render harness (tests/host/render.c, the firmware's own engine): the mix, then the
# pulse and the bass alone; the metrics as JSON and a table (tools/metrics.py), the targets checked.
#   tools/render_all.sh SECONDS OUTDIR [--no-wav]
# --no-wav: the long stability runs (no audio kept, the layers alone skipped: faults and the output stage only).
set -eu
cd "$(dirname "$0")/.."
SECS="${1:-60}"
OUT="${2:-app/build/render}"
WAV=1
[ "${3:-}" = "--no-wav" ] && WAV=0
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
mkdir -p "$OUT"
$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Werror -DEN_HOST -Iapp/dsp -Ibase/include \
    -o "$OUT/render" tests/host/render.c app/dsp/amb.c -lm
for p in 0 1 2 3 4 5 6 7; do
    if [ $WAV -eq 1 ]; then
        "$OUT/render" $p "$SECS" --wav "$OUT/piece$p.wav" --json "$OUT/piece$p.json" >/dev/null
        "$OUT/render" $p "$SECS" --master 4096 --json "$OUT/piece$p-full.json" >/dev/null
        "$OUT/render" $p "$SECS" --solo pulse --json "$OUT/piece$p-pulse.json" >/dev/null
        "$OUT/render" $p "$SECS" --solo bass --json "$OUT/piece$p-bass.json" >/dev/null
    else
        "$OUT/render" $p "$SECS" --json "$OUT/piece$p.json" >/dev/null
    fi
    echo "  rendered piece $p ($SECS s)"
done
"$PY" tools/metrics.py "$OUT" $([ $WAV -eq 1 ] || echo --stability)
