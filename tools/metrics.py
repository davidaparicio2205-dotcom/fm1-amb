#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 David (davidaparicio2205-dotcom)
"""The render harness's metrics (tools/render_all.sh) as a Markdown table, checked against the sound targets of
docs/ERA_BIBLE.md (Acceptance). Exit 1 if a target is missed. Standard library only.

  tools/metrics.py OUTDIR [--stability]
"""
import json
import statistics
import sys
from pathlib import Path

# the targets (docs/ERA_BIBLE.md, Acceptance)
PEAK_MAX_DBFS = -1.0            # the nominal render (MASTER at the simulator's half) stays clear of full scale
DC_MAX = 1e-3                   # |mean| a channel
LOUD_TOL_DB = 2.0               # every piece within +-2 dB of the eight's mean (integrated, gated, K-weighted)
HARSH_MAX = 0                   # tonal peaks in 2..6 kHz over 12 dB above their neighbours and within 30 dB of the top
KICK_HZ = (40.0, 65.0)          # the kick's fundamental, where a piece has one
BAR_SIM_MIN = 0.98              # the pulse's bars alike (cosine of their sixteenths' energies)
BASS_ATTACK_MIN_S = 0.30        # "slow": every rise of the bass's envelope takes at least this, 10..90 %
BASS_SUSTAIN_MIN = 0.70         # 2 s after its peak, the bass keeps at least this much of it (a drone, not a pluck)
LOW_BAND_MIN_DB = -12.0         # energy in 45..250 Hz, of the whole: there for small speakers


def load(d, name):
    p = d / name
    return json.loads(p.read_text()) if p.exists() else None


def main():
    d = Path(sys.argv[1])
    stability = "--stability" in sys.argv
    rows, fails = [], []
    mix = [load(d, f"piece{p}.json") for p in range(8)]
    louds = [m["lufs"] for m in mix]
    mean = statistics.fmean(louds)
    for p, m in enumerate(mix):
        n = m["name"]
        if m["nonfinite"] or m["denormal"]:
            fails.append(f"{n}: {m['nonfinite']} non-finite, {m['denormal']} denormal samples")
        if m["limiter_sat_frac"] > 0:
            fails.append(f"{n}: the limiter saturated ({m['limiter_sat_frac']:.6f} of the samples)")
        if m["peak_dbfs"] > PEAK_MAX_DBFS:
            fails.append(f"{n}: peak {m['peak_dbfs']:.2f} dBFS")
        if max(abs(m["dc_l"]), abs(m["dc_r"])) > DC_MAX:
            fails.append(f"{n}: DC {m['dc_l']:.5f} {m['dc_r']:.5f}")
        if abs(m["lufs"] - mean) > LOUD_TOL_DB:
            fails.append(f"{n}: loudness {m['lufs']:.1f} LUFS, {m['lufs'] - mean:+.1f} dB from the mean {mean:.1f}")
        row = {"Piece": f"{p + 1} {n}", "LUFS": f"{m['lufs']:.1f}", "Peak": f"{m['peak_dbfs']:.1f}",
               "True peak": f"{m['true_peak_dbfs']:.1f}", "Lim. in": f"{m['limiter_max_in']:.2f}",
               "DC": f"{max(abs(m['dc_l']), abs(m['dc_r'])):.1e}", "NaN/den.": f"{m['nonfinite']}/{m['denormal']}",
               "Centroid": f"{m['centroid_hz']:.0f}"}
        if not stability:
            full = load(d, f"piece{p}-full.json")
            pulse = load(d, f"piece{p}-pulse.json")
            bass = load(d, f"piece{p}-bass.json")
            if m["harsh_peaks"] > HARSH_MAX:
                fails.append(f"{n}: {m['harsh_peaks']} harsh peaks in 2..6 kHz (loudest {m['tonal_peak_hz']:.0f} Hz,"
                             f" {m['tonal_peak_prom_db']:.1f} dB over its neighbours, {m['tonal_peak_rel_db']:.1f} dB"
                             f" from the top)")
            if m["band_45_250"] < LOW_BAND_MIN_DB:
                fails.append(f"{n}: 45..250 Hz {m['band_45_250']:.1f} dB")
            has_pulse = pulse["peak_dbfs"] > -60.0 and pulse["kick_hz"] > 0.0 and pulse["bars"] > 2
            if has_pulse and pulse["band_20_45"] + pulse["band_45_250"] > -60.0:
                if not KICK_HZ[0] <= pulse["kick_hz"] <= KICK_HZ[1]:
                    fails.append(f"{n}: kick {pulse['kick_hz']:.1f} Hz")
                if pulse["bar_similarity"] < BAR_SIM_MIN:
                    fails.append(f"{n}: the pulse's bars differ (similarity {pulse['bar_similarity']:.4f})")
            if bass["bass_rises"]:
                if bass["bass_attack_min_s"] < BASS_ATTACK_MIN_S:
                    fails.append(f"{n}: a bass rise in {bass['bass_attack_min_s']:.2f} s")
                if bass["bass_sustain_2s"] < BASS_SUSTAIN_MIN:
                    fails.append(f"{n}: the bass keeps {bass['bass_sustain_2s']:.2f} of its peak after 2 s")
            row.update({
                "Harsh": str(m["harsh_peaks"]),
                "Top tonal 2-6k": f"{m['tonal_peak_hz']:.0f} Hz {m['tonal_peak_rel_db']:.0f} dB",
                "20-45": f"{m['band_20_45']:.0f}", "45-250": f"{m['band_45_250']:.0f}",
                "250-2k": f"{m['band_250_2k']:.0f}", "2-6k": f"{m['band_2k_6k']:.0f}",
                "6-16k": f"{m['band_6k_16k']:.0f}",
                "Kick Hz": f"{pulse['kick_hz']:.1f}" if has_pulse else "-",
                "Bars alike": f"{pulse['bar_similarity']:.3f}" if has_pulse else "-",
                "Bass att. s": f"{bass['bass_attack_min_s']:.2f}" if bass["bass_rises"] else "-",
                "Bass sus.": f"{bass['bass_sustain_2s']:.2f}" if bass["bass_rises"] else "-",
                "Full-MASTER lim. in": f"{full['limiter_max_in']:.2f}",
                "Host CPU %": f"{m['engine_cpu_pct_host']:.2f}"})
        rows.append(row)
    secs = mix[0]["seconds"]
    out = [f"### Render metrics: {secs:.0f} s a piece{' (stability)' if stability else ''}", "",
           f"Loudness: mean {mean:.1f} LUFS, spread {max(louds) - min(louds):.1f} dB "
           f"(target: every piece within +-{LOUD_TOL_DB:.0f} dB of the mean).", ""]
    keys = list(rows[0].keys())
    out.append("| " + " | ".join(keys) + " |")
    out.append("|" + "---|" * len(keys))
    for r in rows:
        out.append("| " + " | ".join(r[k] for k in keys) + " |")
    out.append("")
    out.append("**All targets met.**" if not fails else "**Targets missed:**")
    out += [f"- {f}" for f in fails]
    text = "\n".join(out) + "\n"
    (d / "metrics.md").write_text(text)
    print(text)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
