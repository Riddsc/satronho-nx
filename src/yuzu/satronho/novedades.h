// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QString>

namespace Satronho {

/// One change of a build: "nuevo", "mejora" or "arreglo", and its text.
struct Change {
    QString type;
    QString text;
};

/// What a build brought, from dist/satronho/novedades.json (newest first).
struct BuildNotes {
    QString tag;
    QString date;
    QString title;
    std::vector<Change> changes;
};

/// Every build's notes, newest first. Empty if the resource is missing or broken.
const std::vector<BuildNotes>& AllNotes();

/// This build's release tag ("build-30"), or empty in a local build.
QString CurrentBuildTag();

/// The notes of `tag`, or nullptr.
const BuildNotes* NotesFor(const QString& tag);

/// The notes as rich text: one card per build, with coloured labels.
QString NotesHtml(const std::vector<BuildNotes>& notes);

/// Whether this build's notes have not been shown yet (a newly installed build).
bool HasUnseenNotes();

/// Marks this build's notes as shown.
void MarkNotesSeen();

} // namespace Satronho
