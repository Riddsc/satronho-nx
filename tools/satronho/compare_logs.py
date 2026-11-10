#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Satronho NX
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compares Satronho NX play sessions from their logs (eden_log.txt).

Each build is judged on the same route; this prints the same table every time and warns when two
sessions are not comparable (another console mode, GPU accuracy, resolution or filter, or the first
session after the shader cache changed format).

    python3 tools/satronho/compare_logs.py old_session.txt new_session.txt [...]

Frame times come from the presenter's "last 600 presents" lines. Frames over 100 ms are counted
per minute of play. "Play" starts at the first window slower than 17.5 ms after the game boots and
leaves out stretches pinned at exactly 60 FPS (title screen, menus, pause). Shader numbers come from the pipeline cache and
the slow wait detector; the stutter trace's summaries, when on, add where the time went.
"""

from __future__ import annotations

import argparse
import re
import statistics
import sys
from dataclasses import dataclass, field

PRESENTS = re.compile(
    r"\[\s*([\d.]+)\][^\n]*last 600 presents: frame (\d+) us \(max (\d+), (\d+) over 100 ms")
SETTING = re.compile(r"LogSettings: (M-|--) ([\w.]+): (.*)$")
TIME = re.compile(r"\[\s*([\d.]+)\]")
CATEGORIES = ("gfx_miss", "compute_miss", "pipeline_wait", "shader_capture", "texture", "buffer",
              "query_fence", "present")

# Settings that change what a session measures.
KEY_SETTINGS = {
    "System.use_docked_mode": "Modo",
    "Renderer.gpu_accuracy": "Precisión GPU",
    "Renderer.resolution_setup": "Resolución",
    "Renderer.scaling_filter": "Filtro",
    "Renderer.anti_aliasing": "Antialiasing",
    "Renderer.native_async_draws": "Dibujos sin pipeline",
    "Renderer.vram_usage_mode": "VRAM",
    "Renderer.precache_shaders": "Precache",
}


@dataclass
class Session:
    path: str
    build: str = "?"
    settings: dict[str, str] = field(default_factory=dict)
    seconds: float = 0.0
    windows: list[tuple[float, int, int]] = field(default_factory=list)  # (time, frame us, >100)
    slow_waits: list[int] = field(default_factory=list)
    worker_waits: list[int] = field(default_factory=list)
    cache_migrated: bool = False
    programs_lowered: int | None = None
    stutter_trace: bool = False
    category_ms: dict[str, float] = field(default_factory=dict)
    summary_windows: int = 0
    forced_stop: bool = False

    # Play windows: from the first one slower than the title screen on, without the stretches
    # pinned at exactly 60 FPS (title screen, menus, pause or the game left idle): three or more
    # windows in a row at 16.6-16.8 ms with no slow frame.
    def play(self) -> list[tuple[float, int, int]]:
        start = next((i for i, (_, frame_us, _) in enumerate(self.windows) if frame_us > 17500),
                     None)
        if start is None:
            return []
        windows = [w for w in self.windows[start:] if w[1] < 1_000_000]
        idle = [16600 <= frame_us <= 16800 and over == 0 for _, frame_us, over in windows]
        keep = [True] * len(windows)
        index = 0
        while index < len(windows):
            end = index
            while end < len(windows) and idle[end]:
                end += 1
            if end - index >= 3:
                for k in range(index, end):
                    keep[k] = False
            index = max(end, index + 1)
        return [w for w, k in zip(windows, keep) if k]

    def idle_minutes(self) -> float:
        start = next((i for i, (_, frame_us, _) in enumerate(self.windows) if frame_us > 17500),
                     len(self.windows))
        played = len([w for w in self.windows[start:] if w[1] < 1_000_000])
        return (played - len(self.play())) * 10 / 60


def parse(path: str) -> Session:
    session = Session(path)
    with open(path, encoding="utf-8", errors="replace") as file:
        for line in file:
            if "Satronho NX Version" in line or "Lichii Version" in line:
                match = re.search(r"\b(?:main-)?(build-\d+)", line)
                if match:
                    session.build = match.group(1)
            if "LogSettings:" in line:
                match = SETTING.search(line.rstrip())
                if match and match.group(2) in KEY_SETTINGS:
                    session.settings[match.group(2)] = match.group(3).strip()
            if "last 600 presents" in line:
                match = PRESENTS.search(line)
                if match:
                    session.windows.append(
                        (float(match.group(1)), int(match.group(2)), int(match.group(4))))
            elif "Slow GPU wait:" in line:
                match = re.search(r"Slow GPU wait: (\d+) ms", line)
                if match:
                    session.slow_waits.append(int(match.group(1)))
            elif "kept the Vulkan worker waiting" in line:
                match = re.search(r"waiting (\d+) ms", line)
                if match:
                    session.worker_waits.append(int(match.group(1)))
            elif "started from the version" in line and "Shader cache v2" in line:
                session.cache_migrated = True
            elif "Shader cache v2 programs:" in line:
                match = re.search(r"(\d+) lowered", line)
                if match:
                    session.programs_lowered = int(match.group(1))
            elif "Nx2StutterSummary" in line:
                session.stutter_trace = True
                session.summary_windows += 1
                match = re.search(r"cat_ms=(\S+)", line)
                if match:
                    for name, value in re.findall(r"(\w+):([\d.]+)", match.group(1)):
                        session.category_ms[name] = session.category_ms.get(name, 0.0) + float(value)
            elif "failed to stop, terminating" in line:
                session.forced_stop = True
            match = TIME.match(line)
            if match:
                session.seconds = max(session.seconds, float(match.group(1)))
    return session


def fps_stats(windows: list[tuple[float, int, int]]) -> tuple[float, float, float]:
    """Median FPS, the FPS of the slowest tenth of windows, and frames over 100 ms per minute."""
    if not windows:
        return 0.0, 0.0, 0.0
    fps = sorted(1e6 / frame_us for _, frame_us, _ in windows)
    minutes = len(windows) * 10.0 / 60.0  # A window is about 10 s of presents.
    long_frames = sum(over for _, _, over in windows)
    return statistics.median(fps), fps[len(fps) // 10], long_frames / minutes


def warnings(sessions: list[Session]) -> list[str]:
    notes = []
    for key, label in KEY_SETTINGS.items():
        values = {s.settings.get(key, "(por defecto)") for s in sessions}
        if len(values) > 1 and key not in ("Renderer.precache_shaders",):
            notes.append(f"{label} distinto entre sesiones: {', '.join(sorted(values))}.")
    for s in sessions:
        name = s.path.rsplit("/", 1)[-1]
        if s.cache_migrated:
            notes.append(f"{name}: primera sesión con la caché de shaders nueva (migración); "
                         "se arman shaders que en la siguiente ya salen de la caché.")
        elif s.programs_lowered:
            notes.append(f"{name}: {s.programs_lowered} programas sin registro en la caché: "
                         "zonas nuevas o caché recién creada.")
        if s.stutter_trace:
            notes.append(f"{name}: con el rastreador de tirones activado.")
        if s.forced_stop:
            notes.append(f"{name}: al cerrar, el emulador tuvo que forzar la detención.")
    durations = [len(s.play()) for s in sessions if s.play()]
    if durations and max(durations) > 2.5 * min(durations):
        notes.append("Las sesiones duran muy distinto; compara por recorrido, no por promedio.")
    return notes


def table(sessions: list[Session]) -> str:
    header = ["", *(s.path.rsplit("/", 1)[-1] for s in sessions)]
    rows: list[list[str]] = []

    def row(label: str, values: list[str]) -> None:
        rows.append([label, *values])

    row("Build", [s.build for s in sessions])
    for key, label in KEY_SETTINGS.items():
        row(label, [s.settings.get(key, "(por defecto)") for s in sessions])
    row("Minutos jugando", [f"{len(s.play()) * 10 / 60:.1f}" for s in sessions])
    row("Minutos quieto (menú, pausa, título)", [f"{s.idle_minutes():.1f}" for s in sessions])
    stats = [fps_stats(s.play()) for s in sessions]
    row("FPS típicos", [f"{m:.1f}" for m, _, _ in stats])
    row("FPS del 10 % más lento", [f"{p:.1f}" for _, p, _ in stats])
    row("Cuadros de más de 100 ms por minuto", [f"{l:.1f}" for _, _, l in stats])
    row("Pausas largas (espera a la GPU de más de 1/4 s)",
        [f"{len(s.slow_waits)} (máx. {max(s.slow_waits, default=0)} ms)" for s in sessions])
    row("Shaders que frenaron al emulador más de 100 ms",
        [f"{len(s.worker_waits)} (máx. {max(s.worker_waits, default=0)} ms)" for s in sessions])
    if any(s.stutter_trace for s in sessions):
        for name in CATEGORIES:
            row(f"ms por 10 s en {name}",
                [f"{s.category_ms.get(name, 0.0) / s.summary_windows:.0f}"
                 if s.summary_windows else "—" for s in sessions])

    widths = [max(len(r[i]) for r in [header, *rows]) for i in range(len(header))]
    lines = ["| " + " | ".join(h.ljust(w) for h, w in zip(header, widths)) + " |",
             "|" + "|".join("-" * (w + 2) for w in widths) + "|"]
    lines += ["| " + " | ".join(c.ljust(w) for c, w in zip(r, widths)) + " |" for r in rows]
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("logs", nargs="+", help="eden_log.txt files, oldest first")
    args = parser.parse_args(argv)
    sessions = [parse(path) for path in args.logs]
    print(table(sessions))
    notes = warnings(sessions)
    if notes:
        print("\nAvisos:")
        for note in notes:
            print(f"- {note}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
