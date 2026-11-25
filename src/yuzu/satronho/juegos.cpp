// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/satronho/juegos.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QVBoxLayout>

#include "common/fs/fs_paths.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "yuzu/satronho/directo.h"
#include "yuzu/satronho/guides.h"
#include "yuzu/satronho/session.h"

namespace Satronho::Juegos {

namespace {

constexpr int HistorySize = 12;

QString Tr(const char* text) {
    return QCoreApplication::translate("Satronho::Juegos", text);
}

/// satronho_juegos.ini in the config folder: one group per game.
QSettings& Store() {
    static QSettings settings(
        QString::fromStdString(Common::FS::PathToUTF8String(
            Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "satronho_juegos.ini")),
        QSettings::IniFormat);
    return settings;
}

QString Group(u64 program_id) {
    return QStringLiteral("%1").arg(program_id, 16, 16, QLatin1Char('0')).toUpper();
}

QString Encode(const Record& r) {
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
        .arg(r.when.toSecsSinceEpoch())
        .arg(r.typical_fps, 0, 'f', 1)
        .arg(r.slow_fps, 0, 'f', 1)
        .arg(r.stutters)
        .arg(r.shader_stutters)
        .arg(r.minutes, 0, 'f', 2)
        .arg(r.stream_mode ? 1 : 0);
}

std::optional<Record> Decode(const QString& text) {
    const QStringList parts = text.split(QLatin1Char('|'));
    if (parts.size() < 7) {
        return std::nullopt;
    }
    Record r;
    r.when = QDateTime::fromSecsSinceEpoch(parts[0].toLongLong());
    r.typical_fps = parts[1].toDouble();
    r.slow_fps = parts[2].toDouble();
    r.stutters = parts[3].toInt();
    r.shader_stutters = parts[4].toInt();
    r.minutes = parts[5].toDouble();
    r.stream_mode = parts[6] == QStringLiteral("1");
    return r;
}

QString Duration(double minutes) {
    const int total = static_cast<int>(std::round(minutes));
    if (total >= 60) {
        return Tr("%1 h %2 min").arg(total / 60).arg(total % 60);
    }
    return Tr("%1 min").arg(std::max(total, minutes > 0.0 ? 1 : 0));
}

QColor FpsColor(double fps) {
    if (fps >= 28.0) {
        return QColor(0x4A, 0xDE, 0x80);
    }
    if (fps >= 20.0) {
        return QColor(0xFF, 0xB2, 0x3E);
    }
    return QColor(0xF8, 0x71, 0x71);
}

/// The last sessions' typical FPS as bars, oldest on the left, colored like the meter.
class FpsChart : public QWidget {
public:
    FpsChart(std::vector<Record> sessions, QWidget* parent)
        : QWidget{parent}, sessions{std::move(sessions)} {
        std::reverse(this->sessions.begin(), this->sessions.end());
        setMinimumHeight(110);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (sessions.empty()) {
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const double label = 16.0;
        const double gap = 8.0;
        double max = 60.0;
        for (const Record& r : sessions) {
            max = std::max(max, r.typical_fps);
        }
        const double bar = (width() - gap * (sessions.size() + 1)) / sessions.size();
        const double chart = height() - label - 4.0;
        QFont font = painter.font();
        font.setPointSizeF(8.5);
        painter.setFont(font);
        for (size_t i = 0; i < sessions.size(); ++i) {
            const Record& r = sessions[i];
            const double h = r.typical_fps / max * chart;
            const QRectF rect(gap + i * (bar + gap), 4.0 + chart - h, bar, h);
            QPainterPath path;
            path.addRoundedRect(rect, 6, 6);
            painter.fillPath(path, FpsColor(r.typical_fps));
            painter.setPen(palette().color(QPalette::PlaceholderText));
            painter.drawText(QRectF(rect.left(), height() - label, bar, label), Qt::AlignCenter,
                             QString::number(r.typical_fps, 'f', 0));
        }
    }

private:
    std::vector<Record> sessions;
};

QString SavesFolderName(const std::filesystem::path& save_dir) {
    return QString::fromStdString(Common::FS::PathToUTF8String(save_dir.filename()));
}

QString BackupFolder() {
    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QDir dir(documents);
    dir.mkpath(QStringLiteral("Satronho NX/Respaldos"));
    return dir.filePath(QStringLiteral("Satronho NX/Respaldos"));
}

QString SafeName(QString title) {
    static const QString bad = QStringLiteral("\\/:*?\"<>|");
    for (QChar& c : title) {
        if (bad.contains(c)) {
            c = QLatin1Char('_');
        }
    }
    return title.trimmed();
}

/// Runs a short archive command; true on success.
bool Run(const QString& program, const QStringList& arguments) {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(120000)) {
        process.kill();
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        LOG_WARNING(Frontend, "Satronho saves: {} failed: {}", program.toStdString(),
                    QString::fromLocal8Bit(process.readAllStandardError()).toStdString());
        return false;
    }
    return true;
}

bool Zip(const QString& zip, const QString& parent, const QString& folder) {
#ifdef _WIN32
    // Windows 10 and 11 ship bsdtar, which writes .zip with -a.
    return Run(QStringLiteral("tar"), {QStringLiteral("-a"), QStringLiteral("-c"),
                                       QStringLiteral("-f"), zip, QStringLiteral("-C"), parent,
                                       folder});
#else
    QProcess process;
    process.setWorkingDirectory(parent);
    process.start(QStringLiteral("zip"), {QStringLiteral("-r"), QStringLiteral("-q"), zip, folder});
    return process.waitForFinished(120000) && process.exitCode() == 0;
#endif
}

bool Unzip(const QString& zip, const QString& into) {
#ifdef _WIN32
    return Run(QStringLiteral("tar"),
               {QStringLiteral("-x"), QStringLiteral("-f"), zip, QStringLiteral("-C"), into});
#else
    return Run(QStringLiteral("unzip"), {QStringLiteral("-q"), QStringLiteral("-o"), zip,
                                         QStringLiteral("-d"), into});
#endif
}

bool CopyTree(const QString& from, const QString& to) {
    QDir source(from);
    if (!QDir().mkpath(to)) {
        return false;
    }
    for (const QFileInfo& entry :
         source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
        const QString target = QDir(to).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!CopyTree(entry.filePath(), target)) {
                return false;
            }
        } else {
            QFile::remove(target);
            if (!QFile::copy(entry.filePath(), target)) {
                return false;
            }
        }
    }
    return true;
}

} // Anonymous namespace

void AddSession(const Session::Summary& summary) {
    if (summary.program_id == 0 || summary.minutes < 0.5) {
        return;
    }
    Record record{
        .when = QDateTime::currentDateTime(),
        .typical_fps = summary.typical_fps,
        .slow_fps = summary.slow_fps,
        .stutters = summary.stutters,
        .shader_stutters = summary.shader_stutters,
        .minutes = summary.minutes,
        .stream_mode = summary.stream_mode,
    };
    QSettings& store = Store();
    store.beginGroup(Group(summary.program_id));
    QStringList sessions = store.value(QStringLiteral("sessions")).toStringList();
    sessions.prepend(Encode(record));
    while (sessions.size() > HistorySize) {
        sessions.removeLast();
    }
    store.setValue(QStringLiteral("sessions"), sessions);
    store.setValue(QStringLiteral("minutes"),
                   store.value(QStringLiteral("minutes"), 0.0).toDouble() + summary.minutes);
    store.endGroup();
    store.sync();
}

std::vector<Record> History(u64 program_id) {
    QSettings& store = Store();
    store.beginGroup(Group(program_id));
    const QStringList sessions = store.value(QStringLiteral("sessions")).toStringList();
    store.endGroup();
    std::vector<Record> records;
    for (const QString& text : sessions) {
        if (auto record = Decode(text)) {
            records.push_back(*record);
        }
    }
    return records;
}

double TotalMinutes(u64 program_id) {
    QSettings& store = Store();
    store.beginGroup(Group(program_id));
    const double minutes = store.value(QStringLiteral("minutes"), 0.0).toDouble();
    store.endGroup();
    return minutes;
}

bool Prepared(u64 program_id) {
    return Directo::LastPrepared(program_id).has_value();
}

QString Badge(u64 program_id) {
    if (program_id == 0) {
        return {};
    }
    QStringList parts;
    if (Prepared(program_id)) {
        parts << QStringLiteral("✓");
    }
    const auto history = History(program_id);
    if (!history.empty()) {
        parts << QStringLiteral("%1 fps").arg(history.front().typical_fps, 0, 'f', 0);
    }
    return parts.join(QLatin1Char(' '));
}

QString BackupSaves(const std::filesystem::path& save_dir, const QString& title) {
    const QString saves = QString::fromStdString(Common::FS::PathToUTF8String(save_dir));
    if (!QDir(saves).exists() || QDir(saves).isEmpty()) {
        return {};
    }
    const QString zip = QDir(BackupFolder()).filePath(
        QStringLiteral("%1 %2.zip")
            .arg(SafeName(title),
                 QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH-mm-ss"))));
    const QString parent = QFileInfo(saves).absolutePath();
    if (!Zip(zip, parent, SavesFolderName(save_dir))) {
        return {};
    }
    LOG_INFO(Frontend, "Satronho saves: backed up to {}", zip.toStdString());
    return zip;
}

QString RestoreSaves(const QString& zip, const std::filesystem::path& save_dir,
                     const QString& title) {
    QTemporaryDir temp;
    if (!temp.isValid() || !Unzip(zip, temp.path())) {
        return Tr("No se pudo abrir el .zip.");
    }
    // The program id folder, at the root or one level down.
    const QString name = SavesFolderName(save_dir);
    QString found;
    const QDir root(temp.path());
    if (root.exists(name)) {
        found = root.filePath(name);
    } else {
        for (const QString& sub : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (QDir(root.filePath(sub)).exists(name)) {
                found = QDir(root.filePath(sub)).filePath(name);
                break;
            }
        }
    }
    if (found.isEmpty()) {
        return Tr("Este .zip no tiene partidas de este juego (falta la carpeta %1).").arg(name);
    }
    // What is there now goes to a backup first.
    BackupSaves(save_dir, title + Tr(" (antes de restaurar)"));
    const QString saves = QString::fromStdString(Common::FS::PathToUTF8String(save_dir));
    QDir(saves).removeRecursively();
    if (!CopyTree(found, saves)) {
        return Tr("No se pudieron copiar las partidas.");
    }
    LOG_INFO(Frontend, "Satronho saves: restored {} into {}", zip.toStdString(),
             saves.toStdString());
    return {};
}

void ShowPage(QWidget* parent, u64 program_id, const QString& title, const QPixmap& icon,
              const std::filesystem::path& save_dir, const Actions& actions) {
    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(Tr("Ficha Satronho · %1").arg(title));
    dialog->setMinimumWidth(520);
    auto* layout = new QVBoxLayout(dialog);
    layout->setSpacing(10);

    // The game's art, name and whether it is prepared.
    auto* header = new QHBoxLayout;
    auto* art = new QLabel(dialog);
    if (!icon.isNull()) {
        art->setPixmap(icon.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    header->addWidget(art);
    auto* names = new QVBoxLayout;
    auto* name = new QLabel(QStringLiteral("<h2>%1</h2>").arg(title.toHtmlEscaped()), dialog);
    name->setWordWrap(true);
    names->addWidget(name);
    const auto prepared = Directo::LastPrepared(program_id);
    auto* status = new QLabel(
        prepared ? Tr("<span style=\"color:#4ADE80\"><b>✓ Preparado</b></span> el %1")
                       .arg(QLocale().toString(prepared->when, QLocale::ShortFormat))
                 : Tr("<span style=\"color:#9B8CFF\"><b>Sin preparar</b></span>"),
        dialog);
    names->addWidget(status);
    names->addStretch();
    header->addLayout(names, 1);
    layout->addLayout(header);

    // Numbers: time played, sessions, the last one's FPS and stutters.
    const auto history = History(program_id);
    auto* stats = new QHBoxLayout;
    const auto stat = [dialog, stats](const QString& value, const QString& label,
                                      const QColor& color) {
        auto* box = new QLabel(QStringLiteral("<div align=\"center\"><span style=\"font-size:16pt;"
                                              "color:%1\"><b>%2</b></span><br>%3</div>")
                                   .arg(color.name(), value.toHtmlEscaped(),
                                        label.toHtmlEscaped()),
                               dialog);
        stats->addWidget(box, 1);
    };
    stat(Duration(TotalMinutes(program_id)), Tr("Jugado"), QColor(0x4F, 0xD1, 0xC5));
    stat(QString::number(history.size()), Tr("Sesiones"), QColor(0x9B, 0x8C, 0xFF));
    stat(history.empty() ? QStringLiteral("—")
                         : QString::number(history.front().typical_fps, 'f', 0),
         Tr("FPS última"), QColor(0x4A, 0xDE, 0x80));
    stat(history.empty() ? QStringLiteral("—") : QString::number(history.front().stutters),
         Tr("Tirones"), QColor(0xFF, 0xB2, 0x3E));
    layout->addLayout(stats);

    if (history.empty()) {
        layout->addWidget(new QLabel(
            Tr("Todavía no hay sesiones: juega un rato y aquí verás cómo le va."), dialog));
    } else {
        layout->addWidget(new QLabel(Tr("FPS típicos de las últimas sesiones"), dialog));
        layout->addWidget(new FpsChart(history, dialog));
    }

    // Samy, on saves and moving them to the phone.
    layout->addWidget(MakeGuideBanner(
        GuideId::Samy,
        Tr("«Respaldar partidas» hace un .zip en Documentos\\Satronho NX\\Respaldos. Ese mismo "
           ".zip se importa en la app de Android (propiedades del juego → Datos de guardado → "
           "Importar), y el de Android se restaura aquí."),
        dialog));

    auto* buttons = new QHBoxLayout;
    const auto button = [dialog, buttons](const QString& text, std::function<void()> run) {
        auto* b = new QPushButton(text, dialog);
        QObject::connect(b, &QPushButton::clicked, dialog, [run] { run(); });
        buttons->addWidget(b);
        return b;
    };
    if (actions.play) {
        button(Tr("Jugar"), [dialog, play = actions.play] {
            dialog->close();
            play();
        })->setDefault(true);
    }
    if (actions.prepare) {
        button(Tr("Preparar para directo"), [dialog, prepare = actions.prepare] {
            dialog->close();
            prepare();
        });
    }
    button(Tr("Respaldar partidas"), [dialog, save_dir, title] {
        const QString zip = BackupSaves(save_dir, title);
        if (zip.isEmpty()) {
            QMessageBox::information(dialog, Tr("Respaldar partidas"),
                                     Tr("Este juego todavía no tiene partidas guardadas, o no se "
                                        "pudo crear el .zip."));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(zip).absolutePath()));
    });
    button(Tr("Restaurar…"), [dialog, save_dir, title] {
        const QString zip = QFileDialog::getOpenFileName(
            dialog, Tr("Restaurar partidas"), BackupFolder(), Tr("Respaldo (*.zip)"));
        if (zip.isEmpty()) {
            return;
        }
        if (QMessageBox::question(
                dialog, Tr("Restaurar partidas"),
                Tr("Las partidas de %1 se reemplazan por las del .zip. Antes guardo un respaldo de "
                   "las de ahora. ¿Seguimos?")
                    .arg(title)) != QMessageBox::Yes) {
            return;
        }
        const QString error = RestoreSaves(zip, save_dir, title);
        if (error.isEmpty()) {
            QMessageBox::information(dialog, Tr("Restaurar partidas"), Tr("Listo: partidas restauradas."));
        } else {
            QMessageBox::warning(dialog, Tr("Restaurar partidas"), error);
        }
    });
    if (actions.open_saves) {
        button(Tr("Carpeta de partidas"), actions.open_saves);
    }
    layout->addLayout(buttons);
    dialog->show();
}

} // namespace Satronho::Juegos
