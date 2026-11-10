#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Satronho NX
# SPDX-License-Identifier: GPL-3.0-or-later
"""Satronho NX's checks before a build is published (run by release-binaries.yml).

- The guides' knowledge base answers its question batteries (check_knowledge.py).
- What's new (dist/satronho/novedades.json) is well formed, newest build first.
- Suggested settings never recommend Wait for draws without a built pipeline (builds 32-33 did,
  and every new pipeline froze the game), and the settings migration still restores Bounded.
- Text the user reads says Satronho NX, not Lichii (credits and Lichii's own services aside).
- The log comparator reads a known log the same way (compare_logs.py).

    python3 tools/satronho/check_satronho.py
"""

from __future__ import annotations

import json
import pathlib
import re
import subprocess
import sys

TOOLS = pathlib.Path(__file__).resolve().parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

sys.dont_write_bytecode = True
import compare_logs  # noqa: E402

# Visible text may name Lichii in these (credits, Lichii's compatibility site, the report's
# "fork of Lichii" build line).
BRANDING_ALLOWED = (
    "Credits (Lichii)",
    "fork of Lichii",
)
VISIBLE_DIRS = ("src/yuzu", "src/qt_common", "src/frontend_common", "src/updater")


def check_novedades() -> list[str]:
    problems = []
    data = json.loads((ROOT / "dist/satronho/novedades.json").read_text(encoding="utf-8"))
    numbers = []
    for build in data["builds"]:
        for key in ("tag", "fecha", "titulo", "cambios"):
            if not build.get(key):
                problems.append(f"novedades: {build.get('tag', '?')} sin «{key}»")
        match = re.fullmatch(r"build-(\d+)", build.get("tag", ""))
        if not match:
            problems.append(f"novedades: etiqueta rara {build.get('tag')!r}")
            continue
        numbers.append(int(match.group(1)))
        for change in build.get("cambios", []):
            if change.get("tipo") not in ("nuevo", "mejora", "arreglo"):
                problems.append(f"novedades: {build['tag']} tiene un tipo {change.get('tipo')!r}")
            if not change.get("texto", "").strip():
                problems.append(f"novedades: {build['tag']} tiene un cambio sin texto")
    if numbers != sorted(numbers, reverse=True) or len(set(numbers)) != len(numbers):
        problems.append("novedades: las builds deben ir de la más nueva a la más vieja, sin repetir")
    return problems


def check_settings() -> list[str]:
    problems = []
    presets = (ROOT / "src/yuzu/satronho/presets.cpp").read_text(encoding="utf-8")
    if re.search(r"AddChange\(\s*suggestion,\s*v\.native_async_draws,\s*"
                 r"Settings::NativeAsyncDraws::Wait", presets):
        problems.append("presets.cpp: las sugerencias recomiendan Wait (congela el juego en cada "
                        "pipeline nuevo); debe ser Bounded")
    if not re.search(r"AddChange\(\s*suggestion,\s*v\.native_async_draws,\s*"
                     r"Settings::NativeAsyncDraws::Bounded", presets):
        problems.append("presets.cpp: no encuentro la sugerencia de Bounded")
    config = (ROOT / "src/frontend_common/config.cpp").read_text(encoding="utf-8")
    version = re.search(r"satronho_settings_version = (\d+);", config)
    if not version or int(version.group(1)) < 4:
        problems.append("config.cpp: la migración de ajustes de Satronho debe ser v4 o más")
    if not re.search(r"applied < 4\)\s*\{\s*apply\(Settings::values\.native_async_draws,\s*"
                     r"ToString\(u32\(Settings::NativeAsyncDraws::Bounded\)\)\)", config):
        problems.append("config.cpp: la migración v4 ya no restablece Bounded")
    return problems


def check_branding() -> list[str]:
    problems = []
    literal = re.compile(r'"((?:[^"\\]|\\.)*)"')
    visible = re.compile(r"\bLichii\b(?![.\-_])")
    for directory in VISIBLE_DIRS:
        for path in sorted((ROOT / directory).rglob("*")):
            if path.suffix not in (".cpp", ".h", ".ui") or not path.is_file():
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8",
                                                         errors="replace").splitlines(), 1):
                stripped = line.strip()
                if stripped.startswith(("//", "*", "/*", "#include")):
                    continue
                code = line.split("//")[0] if path.suffix != ".ui" else line
                texts = [m.group(1) for m in literal.finditer(code)]
                if path.suffix == ".ui":
                    texts = re.findall(r"<string>(.*?)</string>", line) or []
                for text in texts:
                    if visible.search(text) and not any(a in text for a in BRANDING_ALLOWED):
                        where = path.relative_to(ROOT)
                        problems.append(f"{where}:{number}: texto visible con «Lichii»: {text[:80]}")
    return problems


def check_comparator() -> list[str]:
    fixture = TOOLS / "fixtures" / "sesion_corta.txt"
    session = compare_logs.parse(str(fixture))
    problems = []
    expected = {
        "build": "build-35",
        "Modo": session.settings.get("System.use_docked_mode") == "Docked",
        "ventanas": len(session.play()) == 3,
        "pausas": session.slow_waits == [1591],
        "shaders": session.worker_waits == [5260],
        "migración": session.cache_migrated,
    }
    if session.build != expected.pop("build"):
        problems.append(f"compare_logs: build leída {session.build!r}")
    for name, ok in expected.items():
        if not ok:
            problems.append(f"compare_logs: no lee bien {name} del registro de prueba")
    median, slowest, long_per_minute = compare_logs.fps_stats(session.play())
    if abs(median - 40.0) > 0.1 or abs(long_per_minute - 6.0) > 0.01:
        problems.append(f"compare_logs: cuentas inesperadas ({median:.1f} FPS, "
                        f"{long_per_minute:.1f} cuadros lentos por minuto)")
    return problems


def main() -> int:
    failures: list[str] = []
    print("== Base de conocimiento de los guías")
    if subprocess.run([sys.executable, str(TOOLS / "check_knowledge.py")]).returncode != 0:
        failures.append("La base de conocimiento no pasa sus pruebas.")
    for title, check in (("Novedades", check_novedades), ("Configuraciones sugeridas",
                         check_settings), ("Marca", check_branding),
                         ("Comparador de registros", check_comparator)):
        problems = check()
        print(f"== {title}: {'bien' if not problems else f'{len(problems)} problema(s)'}")
        for problem in problems:
            print(f"  {problem}")
        failures += problems
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
