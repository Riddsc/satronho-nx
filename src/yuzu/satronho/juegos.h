// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <functional>
#include <vector>

#include <QDateTime>
#include <QPixmap>
#include <QString>

#include "common/common_types.h"

class QWidget;

namespace Satronho::Session {
struct Summary;
}

/// Each game's record in Satronho NX, as on Android: its sessions, the time played, its mark on
/// the game list (✓ when prepared, the last session's FPS) and its page, with a save backup
/// that the Android app can import too.
namespace Satronho::Juegos {

struct Record {
    QDateTime when;
    double typical_fps{};
    double slow_fps{};
    int stutters{};
    int shader_stutters{};
    double minutes{};
    bool stream_mode{};
};

/// Keeps a finished session (from half a minute) in the game's history.
void AddSession(const Session::Summary& summary);

/// The game's last sessions, newest first.
std::vector<Record> History(u64 program_id);

/// Everything played of the game in Satronho NX, minutes.
double TotalMinutes(u64 program_id);

/// The mark on the game's card: "✓", "60 fps", "✓ 60 fps", or empty.
QString Badge(u64 program_id);

/// Whether the game has a finished «Preparar para directo».
bool Prepared(u64 program_id);

/// What the game's page can do, from the main window.
struct Actions {
    std::function<void()> play;
    std::function<void()> prepare;
    std::function<void()> open_saves;
};

/// The game's Satronho page: time played, sessions, a chart of their FPS, and buttons.
/// `save_dir` is the game's save folder (it may not exist yet).
void ShowPage(QWidget* parent, u64 program_id, const QString& title, const QPixmap& icon,
              const std::filesystem::path& save_dir, const Actions& actions);

/// Packs the save folder into Documents/Satronho NX/Respaldos/<game> <date>.zip, with the
/// program id folder at its root as Android's backup has it. Returns the .zip, or empty.
QString BackupSaves(const std::filesystem::path& save_dir, const QString& title);

/// Restores a backup .zip (from here or from the Android app) into the save folder, after
/// backing up what is there now. Returns an error message, or empty on success.
QString RestoreSaves(const QString& zip, const std::filesystem::path& save_dir,
                     const QString& title);

} // namespace Satronho::Juegos
