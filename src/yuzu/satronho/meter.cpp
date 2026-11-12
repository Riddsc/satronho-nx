// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/satronho/meter.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QStringList>
#include <QWindow>

#include "common/memory_detect.h"
#include "common/settings.h"
#include "core/core.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/qt_common.h"
#include "video_core/gpu.h"
#include "yuzu/main_window.h"

namespace Satronho {

namespace {

using Common::DkbCpuTrace::Role;
using Clock = Core::FrameTimeHistory::Clock;

QString Tr(const char* text) {
    return QCoreApplication::translate("Satronho::Meter", text);
}

// Layout at 100 %; the meter is painted scaled by its size option.
constexpr qreal Width = 300;
constexpr qreal Pad = 12;
constexpr qreal Content = Width - Pad - 10;
constexpr qreal Top = 7;
constexpr qreal HeaderHeight = 44;
constexpr qreal GraphHeight = 72;
constexpr qreal GraphLabels = 40;
constexpr qreal LineHeight = 16;
constexpr qreal Bottom = 8;
constexpr qreal CompactHeight = 26;
constexpr qreal CoresHeight = 22;
constexpr qreal Strip = 3;
constexpr qreal Margin = 12;

constexpr auto HistoryWindow = std::chrono::seconds{5};
constexpr auto FrameTimeWindow = std::chrono::milliseconds{500};
constexpr std::size_t FpsSampleCount = 10;
/// A frame this much slower than the median is drawn as a spike.
constexpr float SpikeRatio = 1.2f;
/// The same line Satronho's log comparator draws: frames over 100 ms are stutters.
constexpr float StutterMs = 100.0f;
constexpr double GiB = 1024.0 * 1024.0 * 1024.0;
constexpr double VideoMemoryWarn = 0.85;
constexpr double VideoMemoryFull = 0.97;

constexpr std::array<qreal, 4> Scales{0.85, 1.0, 1.25, 1.5};
constexpr std::array<int, 4> Opacities{30, 55, 80, 95};

// Satronho's night purple and text colours.
const QColor Background{0x1B, 0x18, 0x29};
const QColor Text{0xEB, 0xE8, 0xF5};
const QColor Muted{0xA9, 0xA4, 0xC2};
const QColor Dim{0x6E, 0x6A, 0x86};
const QColor Spike{0xFF, 0x5D, 0x73};
const QColor Warn{0xFF, 0x9F, 0x43};

struct Accent {
    const char* name;
    QColor color;
};
// Satronho's teal first, then the guides' own colours.
const std::array<Accent, 5> Accents{{
    {QT_TRANSLATE_NOOP("Satronho::Meter", "Satronho (verde agua)"), QColor{0x4F, 0xD1, 0xC5}},
    {QT_TRANSLATE_NOOP("Satronho::Meter", "Samy (ámbar)"), QColor{0xFF, 0xB2, 0x3E}},
    {QT_TRANSLATE_NOOP("Satronho::Meter", "Troya (lila)"), QColor{0x9B, 0x8C, 0xFF}},
    {QT_TRANSLATE_NOOP("Satronho::Meter", "MechaRidds (verde)"), QColor{0x74, 0xD1, 0x4C}},
    {QT_TRANSLATE_NOOP("Satronho::Meter", "Juana (rosa)"), QColor{0xFF, 0x8F, 0xB5}},
}};

enum Corner : u32 { TopLeft, TopRight, BottomLeft, BottomRight, Free };

/// The threads whose busiest one is the CPU side's limit: the guest cores and GPU emulation.
constexpr std::array CpuRoles{
    Role::GuestCore0, Role::GuestCore1,      Role::GuestCore2,  Role::GuestCore3, Role::GuestCore4,
    Role::GuestCore5, Role::GuestSingleCore, Role::GpuCommand, Role::GpuBack,
};

auto& Values() {
    return UISettings::values;
}

u32 Rows() {
    return Values().satronho_meter_rows.GetValue();
}

bool Shows(u32 row) {
    return (Rows() & row) != 0;
}

QColor AccentColor() {
    return Accents[std::min<std::size_t>(Values().satronho_meter_color.GetValue(),
                                         Accents.size() - 1)]
        .color;
}

QFont Mono(qreal pixel_size, QFont::Weight weight = QFont::Normal) {
    QFont font;
    font.setFamilies({QStringLiteral("Cascadia Mono"), QStringLiteral("JetBrains Mono"),
                      QStringLiteral("Consolas"), QStringLiteral("SF Mono"),
                      QStringLiteral("Menlo"), QStringLiteral("DejaVu Sans Mono")});
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(static_cast<int>(pixel_size));
    font.setWeight(weight);
    return font;
}

struct Run {
    QString text;
    QFont font;
    QColor color;
};

qreal RunsWidth(const std::vector<Run>& runs) {
    qreal width = 0;
    for (const Run& run : runs) {
        width += QFontMetricsF{run.font}.horizontalAdvance(run.text);
    }
    return width;
}

/// Draws runs of text with their baseline centred in a line of `height` starting at `top`.
void DrawRuns(QPainter& painter, qreal x, qreal top, qreal height, const std::vector<Run>& runs) {
    for (const Run& run : runs) {
        const QFontMetricsF metrics{run.font};
        const qreal baseline =
            top + (height - metrics.ascent() - metrics.descent()) / 2 + metrics.ascent();
        painter.setFont(run.font);
        painter.setPen(run.color);
        painter.drawText(QPointF{x, baseline}, run.text);
        x += metrics.horizontalAdvance(run.text);
    }
}

QString Gigabytes(u64 bytes) {
    return QString::number(static_cast<double>(bytes) / GiB, 'f', 1);
}

QString Percent(const std::optional<double>& value) {
    return value ? QStringLiteral("%1%").arg(std::round(*value), 0, 'f', 0) : QStringLiteral("—");
}

QPixmap Swatch(const QColor& color) {
    QPixmap pixmap{14, 14};
    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QRectF{1, 1, 12, 12});
    return pixmap;
}

} // namespace

struct Meter::Cell {
    std::vector<Run> runs;
};

Meter::Meter(MainWindow* main_window_, AreaFn area_, std::function<void()> save_config_)
    : QWidget(main_window_), main_window{main_window_}, area{std::move(area_)},
      save_config{std::move(save_config_)} {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setWindowTitle(Tr("Medidor Satronho"));
    main_window->installEventFilter(this);
    connect(main_window, &MainWindow::statsUpdated, this, &Meter::UpdateStats);
    // Rows still at build 40's default get the new cores row.
    if (Values().satronho_meter_rows.GetValue() == DefaultRowsBuild40) {
        Values().satronho_meter_rows.SetValue(DefaultRows);
    }
    Relayout();
}

void Meter::ApplySettings() {
    Relayout();
    update();
}

qreal Meter::Scale() const {
    return Scales[std::min<std::size_t>(Values().satronho_meter_size.GetValue(),
                                        Scales.size() - 1)];
}

QSizeF Meter::BaseSize() const {
    if (Values().satronho_meter_compact.GetValue()) {
        const QFontMetricsF metrics{Mono(12)};
        return {Pad + metrics.horizontalAdvance(CompactText()) + 12, CompactHeight};
    }
    qreal height = Top;
    if (Shows(Fps | FrameTime | Load)) {
        height += HeaderHeight;
    }
    if (Shows(Graph)) {
        height += 4 + GraphHeight;
    }
    const auto cells = Cells().size();
    if (cells > 0) {
        height += 6 + LineHeight * static_cast<qreal>((cells + 1) / 2) +
                  2 * static_cast<qreal>((cells + 1) / 2 - 1);
    }
    if (Shows(Cores)) {
        height += 6 + CoresHeight + LineHeight;
    }
    height += Bottom;
    return {Width, std::max<qreal>(height, CompactHeight)};
}

void Meter::Relayout() {
    const QSizeF base = BaseSize();
    const qreal scale = Scale();
    const QSize wanted{static_cast<int>(std::ceil(base.width() * scale)),
                       static_cast<int>(std::ceil(base.height() * scale))};
    if (size() != wanted) {
        setFixedSize(wanted);
    }
    Place();
}

void Meter::Place() {
    if (dragging) {
        return;
    }
    const QRect game = area ? area() : main_window->geometry();
    const int margin = static_cast<int>(Margin * Scale());
    QPoint position;
    switch (Values().satronho_meter_corner.GetValue()) {
    case TopRight:
        position = {game.right() - width() - margin + 1, game.top() + margin};
        break;
    case BottomLeft:
        position = {game.left() + margin, game.bottom() - height() - margin + 1};
        break;
    case BottomRight:
        position = {game.right() - width() - margin + 1, game.bottom() - height() - margin + 1};
        break;
    case Free:
        position = game.topLeft() + QPoint{static_cast<int>(Values().satronho_meter_x.GetValue()),
                                           static_cast<int>(Values().satronho_meter_y.GetValue())};
        position.setX(std::clamp(position.x(), game.left(), std::max(game.left(),
                                                                     game.right() - width())));
        position.setY(std::clamp(position.y(), game.top(), std::max(game.top(),
                                                                    game.bottom() - height())));
        break;
    case TopLeft:
    default:
        position = {game.left() + margin, game.top() + margin};
        break;
    }
    if (pos() != position) {
        move(position);
    }
}

bool Meter::eventFilter(QObject* object, QEvent* event) {
    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::WindowStateChange:
        Place();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(object, event);
}

void Meter::UpdateStats(const Core::PerfStatsResults& results, const VideoCore::ShaderNotify&) {
    auto& system = *QtCommon::system;
    auto& perf_stats = system.GetPerfStats();
    perf_stats.SetFrameTimeHistoryEnabled(isVisible());
    if (!isVisible()) {
        return;
    }

    fps.reset();
    if (!std::isnan(results.average_game_fps)) {
        fps = results.average_game_fps;
        fps_samples.push_back(static_cast<float>(std::round(*fps)));
        if (fps_samples.size() > FpsSampleCount) {
            fps_samples.pop_front();
        }
    }

    frames = perf_stats.GetRecentFrameTimes(HistoryWindow);
    frame_ms.reset();
    if (!frames.empty() && Clock::now() - frames.back().end < std::chrono::seconds{1}) {
        double sum = 0;
        int count = 0;
        for (auto it = frames.rbegin();
             it != frames.rend() && frames.back().end - it->end < FrameTimeWindow; ++it) {
            sum += it->ms;
            ++count;
        }
        frame_ms = sum / count;
    }
    // Stutters: every frame over 100 ms not counted yet.
    for (const auto& frame : frames) {
        if (counted_until && frame.end <= *counted_until) {
            continue;
        }
        if (frame.ms > StutterMs) {
            ++stutters;
            last_stutter = frame.end;
        }
    }
    if (!frames.empty()) {
        counted_until = frames.back().end;
    }

    // CPU: the busiest emulation thread over the update's wall time.
    const auto snapshot = Common::DkbCpuTrace::ReadSnapshot();
    cpu_percent.reset();
    if (cpu_snapshot) {
        const double wall_ns =
            std::chrono::duration<double, std::nano>(snapshot.wall_time - cpu_snapshot->wall_time)
                .count();
        std::optional<u64> busiest;
        for (const Role role : CpuRoles) {
            const auto index = static_cast<std::size_t>(role);
            if (const auto delta = Common::DkbCpuTrace::CpuTimeDelta(cpu_snapshot->threads[index],
                                                                     snapshot.threads[index])) {
                busiest = std::max(busiest.value_or(0), *delta);
            }
        }
        if (busiest && wall_ns > 0) {
            cpu_percent = std::min(100.0, static_cast<double>(*busiest) / wall_ns * 100.0);
        }
        // Each guest core and the GPU back thread on their own, for the cores row.
        const auto busy = [&](Role role) -> std::optional<double> {
            const auto index = static_cast<std::size_t>(role);
            const auto delta = Common::DkbCpuTrace::CpuTimeDelta(cpu_snapshot->threads[index],
                                                                 snapshot.threads[index]);
            if (!delta || wall_ns <= 0) {
                return std::nullopt;
            }
            return std::min(100.0, static_cast<double>(*delta) / wall_ns * 100.0);
        };
        for (std::size_t core = 0; core < core_busy.size(); ++core) {
            core_busy[core] = busy(static_cast<Role>(core));
        }
        if (!core_busy[0]) {
            core_busy[0] = busy(Role::GuestSingleCore);
        }
        gpu_back_busy = busy(Role::GpuBack);
    }
    cpu_snapshot = snapshot;

    // What the game's threads waited on since the last update, by group, smoothed.
    {
        namespace Waits = Satronho::Waits;
        const auto totals = Waits::ReadTotals();
        if (wait_totals) {
            const auto delta = [&](Waits::Kind kind) {
                const auto index = static_cast<std::size_t>(kind);
                return static_cast<double>(totals.ns[index] - wait_totals->ns[index]);
            };
            const std::array<double, 4> now{
                delta(Waits::Kind::GpuRead) + delta(Waits::Kind::GpuSync),
                delta(Waits::Kind::Present),
                delta(Waits::Kind::Threads),
                delta(Waits::Kind::Services) + delta(Waits::Kind::Sleep),
            };
            for (std::size_t i = 0; i < now.size(); ++i) {
                wait_share[i] = wait_share[i] * 0.7 + now[i] * 0.3;
            }
        }
        wait_totals = totals;
    }

    auto& renderer = system.GPU().Renderer();
    renderer.RequestGpuBusySample();
    gpu_percent.reset();
    if (const auto gpu_ms = renderer.GetGpuBusyMs(); gpu_ms && frame_ms && *frame_ms > 0) {
        gpu_percent = std::min(100.0, *gpu_ms / *frame_ms * 100.0);
    }

    // What changes what the meter measures: the same settings Satronho's log comparator checks.
    QStringList tags;
    tags << (Settings::IsDockedMode() ? QStringLiteral("Docked") : QStringLiteral("Handheld"));
    tags << (Settings::IsGPULevelHigh() ? QStringLiteral("Accurate") : QStringLiteral("Fast"));
    if (const QString& dlss = main_window->DlssStatusText(); !dlss.isEmpty()) {
        tags << dlss;
    }
    if (Settings::values.ocarina_fsr.GetValue()) {
        tags << (Settings::values.amd_fsr4.GetValue() ? QStringLiteral("FSR 4")
                                                      : QStringLiteral("FSR 3.1"));
    } else {
        switch (Settings::values.scaling_filter.GetValue()) {
        case Settings::ScalingFilter::Fsr:
            tags << QStringLiteral("FSR 1");
            break;
        case Settings::ScalingFilter::Fsr3:
            tags << QStringLiteral("FSR 3");
            break;
        case Settings::ScalingFilter::Sgsr:
        case Settings::ScalingFilter::SgsrEdge:
            tags << QStringLiteral("SGSR");
            break;
        default:
            break;
        }
    }
    if (const auto& scaling = Settings::values.resolution_info; scaling.active) {
        tags << QStringLiteral("%1×").arg(scaling.up_factor, 0, 'g', 3);
    }
    if (const double generated = main_window->FrameGenFps(); generated >= 0.5) {
        tags << Tr("FG +%1").arg(std::round(generated), 0, 'f', 0);
    }
    effects = tags.join(QStringLiteral(" "));

    const auto process = Common::GetProcessMemoryUsage();
    const u64 ram = process.private_working_set ? process.private_working_set
                                                : process.working_set;
    ram_bytes = ram ? std::optional<u64>{ram} : std::nullopt;
    video_memory = renderer.GetVideoMemoryUsage();
    shaders_building = main_window->ShadersBuildingNow();

    Relayout();
    update();
}

std::vector<Meter::Cell> Meter::Cells() const {
    const QFont font = Mono(11);
    const QFont bold = Mono(11, QFont::DemiBold);
    std::vector<Cell> cells;

    if (Shows(Range)) {
        QString range = QStringLiteral("— — —");
        const std::vector<float> samples(fps_samples.begin(), fps_samples.end());
        if (const auto summary = Core::Summarize(samples)) {
            range = QStringLiteral("%1 %2 %3")
                        .arg(summary->min_ms, 0, 'f', 0)
                        .arg(std::round(summary->median_ms), 0, 'f', 0)
                        .arg(summary->max_ms, 0, 'f', 0);
        }
        cells.push_back({{{range, font, Muted}, {QStringLiteral(" fps"), font, Dim}}});
    }
    if (Shows(Effects)) {
        cells.push_back({{{effects.isEmpty() ? QStringLiteral("—") : effects, font, Muted}}});
    }
    if (Shows(Stutters)) {
        const bool recent = last_stutter && Clock::now() - *last_stutter < std::chrono::seconds{10};
        Cell cell{{{Tr("Tirones "), font, Dim},
                   {QString::number(stutters), bold, recent ? Spike : Text}}};
        const double minutes =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - shown_at).count() /
            60.0;
        if (minutes >= 1.0) {
            cell.runs.push_back(
                {QStringLiteral(" · %1/min").arg(stutters / minutes, 0, 'f', 1), font, Muted});
        }
        cells.push_back(std::move(cell));
    }
    if (Shows(Shaders)) {
        cells.push_back({shaders_building > 0
                             ? std::vector<Run>{{Tr("Shaders "), font, Dim},
                                                {QString::number(shaders_building), bold, Warn},
                                                {Tr(" armándose"), font, Muted}}
                             : std::vector<Run>{{Tr("Shaders "), font, Dim},
                                                {Tr("listos"), font, Muted}}});
    }
    if (Shows(Ram)) {
        Cell cell{{{QStringLiteral("RAM "), font, Dim}}};
        if (ram_bytes) {
            cell.runs.push_back({Gigabytes(*ram_bytes), bold, Text});
            if (const u64 total = Common::GetMemInfo().TotalPhysicalMemory) {
                cell.runs.push_back(
                    {QStringLiteral("/%1").arg(std::round(static_cast<double>(total) / GiB), 0, 'f', 0), font, Muted});
            }
            cell.runs.push_back({QStringLiteral(" GB"), font, Dim});
        } else {
            cell.runs.push_back({QStringLiteral("—"), font, Muted});
        }
        cells.push_back(std::move(cell));
    }
    if (Shows(VideoMemory)) {
        Cell cell{{{QStringLiteral("VRAM "), font, Dim}}};
        if (video_memory && video_memory->budget) {
            const double share = static_cast<double>(video_memory->used) /
                                 static_cast<double>(video_memory->budget);
            const QColor color = share >= VideoMemoryFull   ? Spike
                                 : share >= VideoMemoryWarn ? Warn
                                                            : Text;
            cell.runs.push_back({Gigabytes(video_memory->used), bold, color});
            cell.runs.push_back({QStringLiteral("/") + Gigabytes(video_memory->budget), font,
                                 Muted});
            cell.runs.push_back({QStringLiteral(" GB"), font, Dim});
        } else {
            cell.runs.push_back({QStringLiteral("—"), font, Muted});
        }
        cells.push_back(std::move(cell));
    }
    return cells;
}

QString Meter::CompactText() const {
    QStringList parts;
    if (Shows(Fps)) {
        parts << (fps ? QString::number(std::round(*fps), 'f', 0) : QStringLiteral("—")) +
                     QStringLiteral(" fps");
    }
    if (Shows(FrameTime)) {
        parts << (frame_ms ? QString::number(*frame_ms, 'f', 1) : QStringLiteral("—")) +
                     QStringLiteral(" ms");
    }
    if (Shows(Load)) {
        parts << QStringLiteral("CPU %1 GPU %2").arg(Percent(cpu_percent), Percent(gpu_percent));
    }
    if (Shows(Stutters)) {
        parts << Tr("tirones %1").arg(stutters);
    }
    if (Shows(Shaders) && shaders_building > 0) {
        parts << Tr("shaders %1").arg(shaders_building);
    }
    if (Shows(Ram) && ram_bytes) {
        parts << QStringLiteral("RAM %1 GB").arg(Gigabytes(*ram_bytes));
    }
    if (Shows(VideoMemory) && video_memory && video_memory->budget) {
        parts << QStringLiteral("VRAM %1/%2 GB")
                     .arg(Gigabytes(video_memory->used), Gigabytes(video_memory->budget));
    }
    if (parts.isEmpty()) {
        parts << Tr("Medidor Satronho");
    }
    return parts.join(QStringLiteral(" · "));
}

void Meter::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.scale(Scale(), Scale());
    const QSizeF base = BaseSize();

    QColor background = Background;
    const int opacity = Opacities[std::min<std::size_t>(
        Values().satronho_meter_opacity.GetValue(), Opacities.size() - 1)];
    background.setAlpha(opacity * 255 / 100);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF{QPointF{}, base}, 6.0, 6.0);
    // Satronho's strip on the left edge, in the chosen accent.
    painter.setBrush(AccentColor());
    painter.drawRoundedRect(QRectF{0, 5, Strip, base.height() - 10}, 1.5, 1.5);

    if (Values().satronho_meter_compact.GetValue()) {
        PaintCompact(painter);
    } else {
        PaintFull(painter);
    }
}

void Meter::PaintCompact(QPainter& painter) {
    const QFont font = Mono(12);
    const QFont bold = Mono(12, QFont::DemiBold);
    const QStringList parts = CompactText().split(QStringLiteral(" · "));
    std::vector<Run> runs;
    for (qsizetype i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            runs.push_back({QStringLiteral(" · "), font, Dim});
        }
        // The frame rate leads in the accent; the rest reads as text.
        runs.push_back({parts[i], i == 0 && Shows(Fps) ? bold : font,
                        i == 0 && Shows(Fps) ? AccentColor() : Text});
    }
    DrawRuns(painter, Pad, 0, CompactHeight, runs);
}

void Meter::PaintFull(QPainter& painter) {
    const QColor accent = AccentColor();
    const QFont big = Mono(40, QFont::DemiBold);
    const QFont label = Mono(13);
    const QFont label_bold = Mono(13, QFont::DemiBold);
    const qreal right = Pad + Content;
    qreal y = Top;

    if (Shows(Fps | FrameTime | Load)) {
        if (Shows(Fps)) {
            const std::vector<Run> number{
                {fps ? QString::number(std::round(*fps), 'f', 0) : QStringLiteral("—"), big, Text}};
            DrawRuns(painter, Pad, y, HeaderHeight, number);
            DrawRuns(painter, Pad + RunsWidth(number) + 6, y + 6, HeaderHeight,
                     {{QStringLiteral("fps"), label, accent}});
        }
        const qreal right_top = y + (HeaderHeight - 34) / 2;
        qreal line = right_top;
        if (Shows(FrameTime)) {
            const std::vector<Run> runs{
                {frame_ms ? QString::number(*frame_ms, 'f', 1) : QStringLiteral("—"), label_bold,
                 Text},
                {QStringLiteral(" ms"), label, Muted}};
            DrawRuns(painter, right - RunsWidth(runs), line, 17, runs);
            line += 17;
        }
        if (Shows(Load)) {
            // The higher one is what limits the frame rate: it takes the accent.
            const bool both = cpu_percent && gpu_percent;
            const bool cpu_limit = both && std::round(*cpu_percent) >= std::round(*gpu_percent);
            const bool gpu_limit = both && !cpu_limit;
            const std::vector<Run> runs{
                {QStringLiteral("CPU ") + Percent(cpu_percent), cpu_limit ? label_bold : label,
                 cpu_limit ? accent : Muted},
                {QStringLiteral("  "), label, Muted},
                {QStringLiteral("GPU ") + Percent(gpu_percent), gpu_limit ? label_bold : label,
                 gpu_limit ? accent : Muted}};
            DrawRuns(painter, right - RunsWidth(runs), line, 17, runs);
        }
        y += HeaderHeight;
    }
    if (Shows(Graph)) {
        y += 4;
        PaintGraph(painter, QRectF{Pad, y, Content, GraphHeight});
        y += GraphHeight;
    }
    const auto cells = Cells();
    if (!cells.empty()) {
        y += 6;
        for (std::size_t i = 0; i < cells.size(); i += 2) {
            DrawRuns(painter, Pad, y, LineHeight, cells[i].runs);
            if (i + 1 < cells.size()) {
                DrawRuns(painter, right - RunsWidth(cells[i + 1].runs), y, LineHeight,
                         cells[i + 1].runs);
            }
            y += LineHeight + 2;
        }
    }
    if (Shows(Cores)) {
        y += 4;
        PaintCores(painter, y);
    }
}

void Meter::PaintCores(QPainter& painter, qreal top) {
    // One bar per guest core and one for the emulated GPU: a full bar is a host core kept busy the
    // whole time. A frame rate held back by one full bar is limited by that thread; with no full
    // bar, by threads waiting on each other.
    const QColor accent = AccentColor();
    const QFont font = Mono(11);
    constexpr qreal bar_width = 14;
    constexpr qreal gap = 5;
    const qreal bar_top = top + 3;
    const qreal bar_height = CoresHeight - 6;
    const auto bar = [&](qreal x, const std::optional<double>& value) {
        QColor track = Muted;
        track.setAlpha(45);
        painter.setPen(Qt::NoPen);
        painter.setBrush(track);
        painter.drawRoundedRect(QRectF{x, bar_top, bar_width, bar_height}, 2, 2);
        if (!value) {
            return;
        }
        const qreal filled = bar_height * std::clamp(*value, 0.0, 100.0) / 100.0;
        painter.setBrush(*value >= 95.0 ? Warn : accent);
        painter.drawRoundedRect(QRectF{x, bar_top + bar_height - filled, bar_width, filled}, 2, 2);
    };
    const std::vector<Run> label{{Tr("Núcleos "), font, Dim}};
    DrawRuns(painter, Pad, top, CoresHeight, label);
    qreal x = Pad + RunsWidth(label) + 2;
    for (const auto& value : core_busy) {
        bar(x, value);
        x += bar_width + gap;
    }
    const std::vector<Run> gpu{{QStringLiteral("GPU "), font, Dim}};
    const qreal gpu_x = Pad + Content - bar_width - RunsWidth(gpu);
    DrawRuns(painter, gpu_x, top, CoresHeight, gpu);
    bar(Pad + Content - bar_width, gpu_back_busy);

    // Under the bars: what the game's threads wait on most (Satronho's wait measurement).
    static const std::array<const char*, 4> groups{
        QT_TRANSLATE_NOOP("Satronho::Meter", "la GPU"),
        QT_TRANSLATE_NOOP("Satronho::Meter", "la pantalla"),
        QT_TRANSLATE_NOOP("Satronho::Meter", "sus otros hilos"),
        QT_TRANSLATE_NOOP("Satronho::Meter", "los servicios"),
    };
    const auto most = std::max_element(wait_share.begin(), wait_share.end());
    const double sum = wait_share[0] + wait_share[1] + wait_share[2] + wait_share[3];
    std::vector<Run> waits{{Tr("Esperan sobre todo a "), font, Dim}};
    if (sum > 0) {
        waits.push_back({Tr(groups[static_cast<std::size_t>(most - wait_share.begin())]),
                         Mono(11, QFont::DemiBold), accent});
        waits.push_back({QStringLiteral(" (%1 %)").arg(*most / sum * 100.0, 0, 'f', 0), font,
                         Muted});
    } else {
        waits.back().text = Tr("Esperas: —");
    }
    DrawRuns(painter, Pad, top + CoresHeight, LineHeight, waits);
}

void Meter::PaintGraph(QPainter& painter, const QRectF& plot) {
    const QColor accent = AccentColor();
    const qreal plot_width = plot.width() - GraphLabels;
    const qreal axis_x = plot.left() + plot_width + 3;
    QColor axis = Muted;
    axis.setAlpha(60);
    painter.fillRect(QRectF{axis_x, plot.top(), 1, plot.height()}, axis);
    if (frames.size() < 2) {
        return;
    }

    std::vector<float> times;
    times.reserve(frames.size());
    for (const auto& frame : frames) {
        times.push_back(frame.ms);
    }
    const auto summary = *Core::Summarize(times);
    float low = summary.min_ms;
    float high = summary.max_ms;
    // A steady frame rate keeps some span so label jitter stays flat.
    const float min_span = std::max(2.0f, summary.median_ms * 0.1f);
    if (high - low < min_span) {
        const float middle = (low + high) / 2;
        low = middle - min_span / 2;
        high = middle + min_span / 2;
    }
    const qreal top = plot.top() + 6;
    const qreal bottom = plot.bottom() - 7;
    const auto y_of = [&](float ms) { return top + (high - ms) / (high - low) * (bottom - top); };
    const auto newest = frames.back().end;
    const double window = std::chrono::duration<double>(HistoryWindow).count();

    // Under the line, a soft fill in the accent.
    QPolygonF line;
    line.reserve(static_cast<qsizetype>(frames.size()));
    for (const auto& frame : frames) {
        const double age = std::chrono::duration<double>(newest - frame.end).count();
        line << QPointF{plot.left() + plot_width * (1.0 - age / window), y_of(frame.ms)};
    }
    QPolygonF area_fill = line;
    area_fill << QPointF{line.back().x(), bottom} << QPointF{line.front().x(), bottom};
    QLinearGradient gradient{0, top, 0, bottom};
    QColor fill_top = accent;
    fill_top.setAlpha(70);
    QColor fill_bottom = accent;
    fill_bottom.setAlpha(0);
    gradient.setColorAt(0, fill_top);
    gradient.setColorAt(1, fill_bottom);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawPolygon(area_fill);

    painter.setBrush(Qt::NoBrush);
    QColor median_color = accent;
    median_color.setAlpha(130);
    QPen median_pen{median_color, 0.8};
    median_pen.setDashPattern({2.5, 3.5});
    painter.setPen(median_pen);
    const qreal median_y = y_of(summary.median_ms);
    painter.drawLine(QPointF{plot.left(), median_y}, QPointF{plot.left() + plot_width, median_y});

    QColor line_color = Text;
    line_color.setAlpha(225);
    QPen line_pen{line_color, 1.2};
    line_pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(line_pen);
    painter.drawPolyline(line);

    const float spike_ms = summary.median_ms * SpikeRatio;
    QPen spike_pen{Spike, 1.3};
    spike_pen.setCapStyle(Qt::RoundCap);
    painter.setPen(spike_pen);
    for (qsizetype i = 1; i < line.size(); ++i) {
        if (times[i - 1] > spike_ms || times[i] > spike_ms) {
            painter.drawLine(line[i - 1], line[i]);
        }
    }
    // A stutter (over 100 ms) gets a dot on top of its spike.
    painter.setPen(Qt::NoPen);
    painter.setBrush(Spike);
    for (qsizetype i = 0; i < line.size(); ++i) {
        if (times[i] > StutterMs) {
            painter.drawEllipse(line[i], 2.2, 2.2);
        }
    }

    // Max, median and min on the axis, labelled without overlapping.
    struct Tick {
        float ms;
        QColor color;
    };
    const std::array<Tick, 3> ticks{{
        {summary.max_ms, summary.max_ms > spike_ms ? Spike : Text},
        {summary.median_ms, accent},
        {summary.min_ms, Muted},
    }};
    constexpr qreal label_height = 14;
    std::array<qreal, 3> label_tops{};
    for (std::size_t i = 0; i < ticks.size(); ++i) {
        const qreal y = y_of(ticks[i].ms);
        painter.fillRect(QRectF{axis_x - 2, y - 0.5, 5, 1}, ticks[i].color);
        label_tops[i] = y - label_height / 2;
    }
    for (std::size_t i = 1; i < label_tops.size(); ++i) {
        label_tops[i] = std::max(label_tops[i], label_tops[i - 1] + label_height);
    }
    label_tops[2] = std::min(label_tops[2], plot.bottom() + 7 - label_height);
    for (std::size_t i = label_tops.size() - 1; i-- > 0;) {
        label_tops[i] = std::min(label_tops[i], label_tops[i + 1] - label_height);
    }
    const QFont font = Mono(11, QFont::Medium);
    for (std::size_t i = 0; i < ticks.size(); ++i) {
        DrawRuns(painter, axis_x + 7, label_tops[i], label_height,
                 {{QString::number(ticks[i].ms, 'f', 1), font, ticks[i].color}});
    }
}

void Meter::Reset() {
    fps.reset();
    fps_samples.clear();
    frame_ms.reset();
    cpu_percent.reset();
    gpu_percent.reset();
    cpu_snapshot.reset();
    frames.clear();
    ram_bytes.reset();
    video_memory.reset();
    shaders_building = 0;
    core_busy = {};
    gpu_back_busy.reset();
    wait_totals.reset();
    wait_share = {};
    stutters = 0;
    counted_until.reset();
    last_stutter.reset();
    shown_at = std::chrono::steady_clock::now();
}

void Meter::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    Reset();
    Relayout();
}

void Meter::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (QtCommon::system && QtCommon::system->IsPoweredOn()) {
        QtCommon::system->GetPerfStats().SetFrameTimeHistoryEnabled(false);
    }
}

void Meter::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    // Wayland ignores move() on top-level windows: the compositor drags it.
    if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland")) && windowHandle() &&
        windowHandle()->startSystemMove()) {
        return;
    }
    dragging = true;
    drag_offset = event->pos();
}

void Meter::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging || !(event->buttons() & Qt::LeftButton)) {
        return;
    }
    move(event->globalPosition().toPoint() - drag_offset);
}

void Meter::mouseReleaseEvent(QMouseEvent* event) {
    if (!dragging || event->button() != Qt::LeftButton) {
        return;
    }
    dragging = false;
    // Dropped where the user wants it: the meter stays there, relative to the game.
    const QRect game = area ? area() : main_window->geometry();
    const QPoint offset = pos() - game.topLeft();
    Values().satronho_meter_corner.SetValue(Free);
    Values().satronho_meter_x.SetValue(static_cast<u32>(std::max(0, offset.x())));
    Values().satronho_meter_y.SetValue(static_cast<u32>(std::max(0, offset.y())));
    Place();
    if (save_config) {
        save_config();
    }
}

void Meter::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu{this};
    FillOptionsMenu(&menu, [this] {
        ApplySettings();
        if (save_config) {
            save_config();
        }
    });
    menu.addSeparator();
    menu.addAction(Tr("Ocultar el medidor"), this, [this] { close(); });
    menu.exec(event->globalPos());
}

void Meter::closeEvent(QCloseEvent*) {
    emit closed();
}

void Meter::FillOptionsMenu(QMenu* menu, const std::function<void()>& changed) {
    auto& values = Values();

    QMenu* rows = menu->addMenu(Tr("Qué mostrar"));
    const std::array<std::pair<Row, const char*>, 11> row_names{{
        {Fps, QT_TRANSLATE_NOOP("Satronho::Meter", "FPS")},
        {FrameTime, QT_TRANSLATE_NOOP("Satronho::Meter", "Tiempo por cuadro")},
        {Load, QT_TRANSLATE_NOOP("Satronho::Meter", "CPU y GPU")},
        {Graph, QT_TRANSLATE_NOOP("Satronho::Meter", "Gráfica de los últimos 5 segundos")},
        {Range, QT_TRANSLATE_NOOP("Satronho::Meter", "FPS mínimo, típico y máximo")},
        {Effects, QT_TRANSLATE_NOOP("Satronho::Meter", "Modo y efectos activos")},
        {Stutters, QT_TRANSLATE_NOOP("Satronho::Meter", "Tirones (cuadros de más de 100 ms)")},
        {Shaders, QT_TRANSLATE_NOOP("Satronho::Meter", "Shaders armándose")},
        {Ram, QT_TRANSLATE_NOOP("Satronho::Meter", "RAM")},
        {VideoMemory, QT_TRANSLATE_NOOP("Satronho::Meter", "Memoria de video (VRAM)")},
        {Cores, QT_TRANSLATE_NOOP("Satronho::Meter", "Núcleos del juego (barras)")},
    }};
    for (const auto& [row, name] : row_names) {
        QAction* action = rows->addAction(Tr(name));
        action->setCheckable(true);
        action->setChecked((values.satronho_meter_rows.GetValue() & row) != 0);
        QObject::connect(action, &QAction::toggled, menu, [row, changed](bool on) {
            auto& setting = Values().satronho_meter_rows;
            setting.SetValue(on ? setting.GetValue() | row : setting.GetValue() & ~u32{row});
            changed();
        });
    }
    rows->addSeparator();
    QObject::connect(rows->addAction(Tr("Mostrar todo")), &QAction::triggered, menu, [changed] {
        Values().satronho_meter_rows.SetValue(DefaultRows);
        changed();
    });

    // One exclusive submenu per option, each choice an index into its setting.
    const auto choice = [menu, &changed](const QString& title, auto& setting,
                                         const std::vector<QString>& names,
                                         const std::vector<QColor>& swatches = {}) {
        QMenu* sub = menu->addMenu(title);
        auto* group = new QActionGroup(sub);
        for (std::size_t i = 0; i < names.size(); ++i) {
            QAction* action = sub->addAction(names[i]);
            if (i < swatches.size()) {
                action->setIcon(QIcon{Swatch(swatches[i])});
            }
            action->setCheckable(true);
            action->setChecked(setting.GetValue() == i);
            group->addAction(action);
            QObject::connect(action, &QAction::triggered, menu, [&setting, i, changed] {
                setting.SetValue(static_cast<u32>(i));
                changed();
            });
        }
    };

    QMenu* style = menu->addMenu(Tr("Estilo"));
    auto* style_group = new QActionGroup(style);
    const bool compact = values.satronho_meter_compact.GetValue();
    for (const bool option : {false, true}) {
        QAction* action =
            style->addAction(option ? Tr("Compacto (una línea)") : Tr("Completo"));
        action->setCheckable(true);
        action->setChecked(compact == option);
        style_group->addAction(action);
        QObject::connect(action, &QAction::triggered, menu, [option, changed] {
            Values().satronho_meter_compact.SetValue(option);
            changed();
        });
    }
    choice(Tr("Tamaño"), values.satronho_meter_size,
           {Tr("Pequeño"), Tr("Normal"), Tr("Grande"), Tr("Muy grande")});
    choice(Tr("Fondo"), values.satronho_meter_opacity,
           {Tr("Transparente"), Tr("Suave"), Tr("Normal"), Tr("Opaco")});
    std::vector<QString> accent_names;
    std::vector<QColor> accent_colors;
    for (const Accent& accent : Accents) {
        accent_names.push_back(Tr(accent.name));
        accent_colors.push_back(accent.color);
    }
    choice(Tr("Color"), values.satronho_meter_color, accent_names, accent_colors);
    choice(Tr("Posición"), values.satronho_meter_corner,
           {Tr("Arriba a la izquierda"), Tr("Arriba a la derecha"), Tr("Abajo a la izquierda"),
            Tr("Abajo a la derecha"), Tr("Libre (arrástralo)")});

    menu->addSeparator();
    QObject::connect(menu->addAction(Tr("Restablecer el medidor")), &QAction::triggered, menu,
                     [changed] {
                         auto& v = Values();
                         v.satronho_meter_rows.SetValue(DefaultRows);
                         v.satronho_meter_size.SetValue(1);
                         v.satronho_meter_opacity.SetValue(2);
                         v.satronho_meter_color.SetValue(0);
                         v.satronho_meter_corner.SetValue(TopLeft);
                         v.satronho_meter_compact.SetValue(false);
                         changed();
                     });
}

} // namespace Satronho
