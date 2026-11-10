#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Satronho NX
# SPDX-License-Identifier: GPL-3.0-or-later
"""Asks the guides' knowledge base (dist/satronho/conocimiento.json) the questions in kb_tests/.

Mirrors src/yuzu/satronho/knowledge.cpp: the same normalization, stemming, scoring and confidence
rule. Each line of a battery is "question|expected topic id", or "question|-" when the guides
should say they don't know. A battery fails below its floor (FLOORS, else DEFAULT_FLOOR); "nuevas"
holds questions written without looking at the phrasings, so its floor is lower.

    python3 tools/satronho/check_knowledge.py
"""

from __future__ import annotations

import json
import pathlib
import sys
import unicodedata

ROOT = pathlib.Path(__file__).resolve().parents[2]
KNOWLEDGE = ROOT / "dist" / "satronho" / "conocimiento.json"
BATTERIES = pathlib.Path(__file__).resolve().parent / "kb_tests"
DEFAULT_FLOOR = 1.0
FLOORS = {"nuevas": 0.85}

STOPWORDS = set(
    "a al como con cual cuando de del donde el en es esta este hay la las lo los me mi o para pero "
    "por puedo que se si sirve su un una uno y yo tengo hace hacer hago tu le sobre algo eso esto "
    "ese muy mas ya no quiero necesito saber explica explicame dime ayuda hola porfa favor".split())


def normalize(text: str) -> str:
    text = unicodedata.normalize("NFD", text.lower())
    text = "".join(c for c in text if unicodedata.category(c) != "Mn")
    out = []
    for i, c in enumerate(text):
        if c.isalnum():
            out.append(c)
        elif c == "." and 0 < i < len(text) - 1 and text[i - 1].isdigit() and text[i + 1].isdigit():
            out.append(c)
        else:
            out.append(" ")
    return " ".join("".join(out).split())


def stem(word: str) -> str:
    if len(word) > 5 and word.endswith("iones"):
        return word[:-2]
    if len(word) > 4 and word.endswith("es") and not word.endswith("tes"):
        return word[:-2]
    if len(word) > 3 and word.endswith("s"):
        return word[:-1]
    return word


def tokens(text: str) -> list[str]:
    return [stem(w) for w in normalize(text).split() if w not in STOPWORDS]


def phrases(items: list[str]) -> list[str]:
    return [" ".join(stem(w) for w in normalize(x).split()) for x in items if normalize(x)]


class Base:
    def __init__(self, path: pathlib.Path) -> None:
        self.entries = json.loads(path.read_text(encoding="utf-8"))["entradas"]
        for entry in self.entries:
            entry["P"] = phrases(entry["frases"]) + [
                " ".join(stem(w) for w in normalize(entry["pregunta"]).split())]
            entry["K"] = phrases(entry["claves"])
        self.document_frequency: dict[str, int] = {}
        for entry in self.entries:
            for key in set(entry["K"]):
                self.document_frequency[key] = self.document_frequency.get(key, 0) + 1

    def score(self, entry: dict, words: list[str]) -> float:
        if not words:
            return 0.0
        joined = " ".join(words)
        score = 0.0
        for phrase in entry["P"]:
            content = [w for w in phrase.split() if w not in STOPWORDS]
            if not content:
                continue
            found = sum(1 for w in content if w in words)
            if found == len(content):
                score = max(score, 1.0 if len(content) == 1 else 2 + 1.5 * len(content))
            elif len(content) >= 2 and found >= len(content) - 1 and found >= 2:
                score = max(score, 1.5 + found)
            if len(content) >= 2 and " ".join(content) in joined:
                score += 1
        for word in words:
            if word in entry["K"]:
                score += 3 if self.document_frequency.get(word, 0) == 1 else 2
                continue
            for key in entry["K"]:
                if len(word) >= 5 and len(key) >= 5 and (
                        word.startswith(key[:5]) or key.startswith(word[:5])):
                    score += 1
                    break
        return score

    def ask(self, question: str) -> tuple[str | None, list[tuple[float, str]]]:
        words = tokens(question)
        scored = sorted(((self.score(e, words), e["id"]) for e in self.entries),
                        key=lambda x: -x[0])
        scored = [s for s in scored if s[0] > 0]
        if not scored:
            return None, []
        top = scored[0][0]
        second = scored[1][0] if len(scored) > 1 else 0
        confident = ((top >= 4 and (top - second >= 1 or top >= 7)) or
                     (top >= 3 and top - second >= 1) or (top >= 2 and second == 0))
        return (scored[0][1] if confident else None), scored[:3]


def main() -> int:
    base = Base(KNOWLEDGE)
    ids = {e["id"] for e in base.entries}
    failed = False
    for entry in base.entries:
        for linked in entry.get("ver", []):
            if linked not in ids:
                print(f"{entry['id']}: «ver» apunta a un tema que no existe: {linked}")
                failed = True
    for battery in sorted(BATTERIES.glob("*.txt")):
        lines = [l for l in battery.read_text(encoding="utf-8").splitlines() if l.strip()]
        right = 0
        for line in lines:
            question, expected = line.rsplit("|", 1)
            got, top = base.ask(question)
            good = got == expected if expected != "-" else got is None
            right += good
            if not good:
                shown = ", ".join(f"{name} {score:.1f}" for score, name in top)
                print(f"  {battery.stem}: «{question}» -> {got}, esperado {expected} ({shown})")
        floor = FLOORS.get(battery.stem, DEFAULT_FLOOR)
        ok = right >= floor * len(lines)
        print(f"{battery.stem}: {right}/{len(lines)} {'bien' if ok else 'POR DEBAJO DEL MÍNIMO'}")
        failed |= not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
