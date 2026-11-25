// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/satronho/directo.h"

#include <algorithm>
#include <array>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSettings>

#include "common/fs/fs_paths.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "qt_common/config/uisettings.h"
#include "video_core/shader_jobs.h"
#include "yuzu/satronho/novedades.h"

namespace Satronho::Directo {

namespace {

QString Tr(const char* text) {
    return QCoreApplication::translate("Satronho::Directo", text);
}

/// satronho_directo.ini in the config folder: one group per game.
QSettings& Store() {
    static QSettings settings(
        QString::fromStdString(Common::FS::PathToUTF8String(
            Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "satronho_directo.ini")),
        QSettings::IniFormat);
    return settings;
}

QString Group(u64 program_id) {
    return QStringLiteral("%1").arg(program_id, 16, 16, QLatin1Char('0')).toUpper();
}

struct WarmProgress {
    bool seen{};
    u64 done{};
    u64 total{};
};

WarmProgress& Progress() {
    static WarmProgress progress;
    return progress;
}

} // namespace

std::optional<Prepared> LastPrepared(u64 program_id) {
    QSettings& store = Store();
    store.beginGroup(Group(program_id));
    std::optional<Prepared> prepared;
    const QDateTime when = store.value(QStringLiteral("prepared_at")).toDateTime();
    if (when.isValid()) {
        prepared = Prepared{
            .when = when,
            .build_tag = store.value(QStringLiteral("build")).toString(),
            .built = store.value(QStringLiteral("built")).toULongLong(),
        };
    }
    store.endGroup();
    return prepared;
}

bool Pending(u64 program_id) {
    QSettings& store = Store();
    return store.value(Group(program_id) + QStringLiteral("/pending"), false).toBool();
}

void SetPending(u64 program_id, bool pending) {
    QSettings& store = Store();
    store.setValue(Group(program_id) + QStringLiteral("/pending"), pending);
    store.sync();
}

void NoteWarmProgress(u64 done, u64 total) {
    WarmProgress& progress = Progress();
    progress.seen = true;
    progress.done = std::max(progress.done, done);
    progress.total = total;
}

void ResetWarmProgress() {
    Progress() = {};
}

bool FinishLoad(u64 program_id, u64& left) {
    left = 0;
    if (program_id == 0 || !Pending(program_id)) {
        return false;
    }
    const WarmProgress& progress = Progress();
    // No precache stage: the renderer found every compute shader already built for this driver.
    if (progress.seen && progress.done < progress.total) {
        left = progress.total - progress.done;
        LOG_INFO(Frontend, "Satronho directo: {:016X} skipped with {} of {} shaders left",
                 program_id, left, progress.total);
        return false;
    }
    QSettings& store = Store();
    store.beginGroup(Group(program_id));
    store.setValue(QStringLiteral("pending"), false);
    store.setValue(QStringLiteral("prepared_at"), QDateTime::currentDateTime());
    store.setValue(QStringLiteral("build"), CurrentBuildTag());
    store.setValue(QStringLiteral("built"), static_cast<qulonglong>(progress.done));
    store.endGroup();
    store.sync();
    LOG_INFO(Frontend, "Satronho directo: {:016X} prepared ({} compute shaders built)", program_id,
             progress.done);
    return true;
}

QString MenuText(u64 program_id) {
    const auto prepared = LastPrepared(program_id);
    if (!prepared) {
        return Tr("Preparar para directo");
    }
    const QString date =
        QLocale().toString(prepared->when.date(), QStringLiteral("d MMM"));
    if (!CurrentBuildTag().isEmpty() && prepared->build_tag != CurrentBuildTag()) {
        return Tr("Preparar para directo (listo desde el %1, antes de esta build)").arg(date);
    }
    return Tr("Preparar para directo (listo desde el %1)").arg(date);
}

namespace {

struct StreamState {
    bool on{};
    QString app;
};

StreamState& Stream() {
    static StreamState state;
    return state;
}

/// Executable names, lower case, of the streaming apps stream mode looks for.
bool IsStreamApp(const QString& lower) {
    static constexpr std::array names{
        "obs64.exe",         "obs32.exe",          "obs.exe",
        "obs",               "streamlabs obs.exe", "streamlabs desktop.exe",
        "tiktok live studio.exe",
    };
    for (const char* name : names) {
        if (lower == QLatin1String(name)) {
            return true;
        }
    }
    return lower.contains(QLatin1String("tiktok")) && lower.contains(QLatin1String("live"));
}

} // Anonymous namespace

bool StreamAutoEnabled() {
    return UISettings::values.satronho_stream_auto.GetValue();
}

void SetStreamAutoEnabled(bool enabled) {
    UISettings::values.satronho_stream_auto.SetValue(enabled);
}

QString FindStreamApp() {
#ifdef _WIN32
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    QString found;
    for (BOOL more = Process32FirstW(snapshot, &entry); more && found.isEmpty();
         more = Process32NextW(snapshot, &entry)) {
        const QString name = QString::fromWCharArray(entry.szExeFile);
        if (IsStreamApp(name.toLower())) {
            found = name;
        }
    }
    CloseHandle(snapshot);
    return found;
#else
    const QDir proc(QStringLiteral("/proc"));
    for (const QString& pid : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!pid.front().isDigit()) {
            continue;
        }
        QFile comm(proc.filePath(pid + QStringLiteral("/comm")));
        if (!comm.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString name = QString::fromUtf8(comm.readAll()).trimmed();
        if (IsStreamApp(name.toLower())) {
            return name;
        }
    }
    return {};
#endif
}

bool UpdateStreamMode() {
    StreamState& state = Stream();
    const QString app = StreamAutoEnabled() ? FindStreamApp() : QString();
    const bool on = !app.isEmpty();
    if (on == state.on && (!on || app == state.app)) {
        return false;
    }
    state.on = on;
    state.app = app;
    VideoCommon::SetShaderJobStreamLimit(on);
    if (on) {
        LOG_INFO(Frontend, "Satronho stream mode: on ({}); shader builds the game isn't waiting "
                           "for use half the workers",
                 app.toStdString());
    } else {
        LOG_INFO(Frontend, "Satronho stream mode: off");
    }
    return true;
}

bool StreamModeOn() {
    return Stream().on;
}

QString StreamModeApp() {
    return Stream().app;
}

} // namespace Satronho::Directo
