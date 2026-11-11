// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <array>
#include <deque>
#include <functional>
#include <optional>
#include <vector>

#include <QRect>
#include <QString>
#include <QWidget>

#include "common/common_types.h"
#include "common/dkb_cpu_trace.h"
#include "core/perf_stats.h"
#include "video_core/renderer_base.h"

class QMenu;
class MainWindow;

namespace VideoCore {
class ShaderNotify;
}

namespace Satronho {

/// Satronho NX's performance meter: a frameless window over the game with the frame rate, the
/// frame time graph, CPU and GPU load, stutters, RAM and video memory. What it shows, its size,
/// background, accent colour and place are the user's (right click, or the Satronho menu).
class Meter : public QWidget {
    Q_OBJECT

public:
    /// Rows that can be turned on and off (bits of UISettings::values.satronho_meter_rows).
    enum Row : u32 {
        Fps = 1u << 0,
        FrameTime = 1u << 1,
        Load = 1u << 2,
        Graph = 1u << 3,
        Range = 1u << 4,
        Stutters = 1u << 5,
        Effects = 1u << 6,
        Ram = 1u << 7,
        VideoMemory = 1u << 8,
        Shaders = 1u << 9,
        Cores = 1u << 10,
    };
    static constexpr u32 DefaultRows = 0x7FF;
    /// The default before the cores row (build 40): still at it means the user never chose rows.
    static constexpr u32 DefaultRowsBuild40 = 0x3FF;

    /// The game's area on screen, in global coordinates.
    using AreaFn = std::function<QRect()>;

    Meter(MainWindow* main_window, AreaFn area, std::function<void()> save_config);

    /// Reads the settings again (size, rows, place...) after they changed.
    void ApplySettings();

    /// Fills `menu` with the meter's options. `changed` runs after any of them changes.
    static void FillOptionsMenu(QMenu* menu, const std::function<void()>& changed);

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    struct Cell;

    void UpdateStats(const Core::PerfStatsResults& results, const VideoCore::ShaderNotify& shaders);
    void Reset();
    void Place();
    void Relayout();
    qreal Scale() const;
    QSizeF BaseSize() const;
    std::vector<Cell> Cells() const;
    QString CompactText() const;

    void PaintFull(QPainter& painter);
    void PaintCompact(QPainter& painter);
    void PaintGraph(QPainter& painter, const QRectF& plot);
    void PaintCores(QPainter& painter, qreal top);

    MainWindow* main_window;
    AreaFn area;
    std::function<void()> save_config;

    QPoint drag_offset;
    bool dragging = false;

    std::optional<double> fps;
    std::deque<float> fps_samples;
    std::optional<double> frame_ms;
    std::optional<double> cpu_percent;
    std::optional<double> gpu_percent;
    std::optional<Common::DkbCpuTrace::Snapshot> cpu_snapshot;
    std::vector<Core::FrameTimeHistory::Entry> frames;
    QString effects;
    std::optional<u64> ram_bytes;
    std::optional<VideoCore::RendererBase::VideoMemoryUsage> video_memory;
    int shaders_building = 0;
    // How busy each guest core and the emulated GPU's back thread were, in % of a host core.
    std::array<std::optional<double>, 6> core_busy{};
    std::optional<double> gpu_back_busy;

    // Stutters: frames over 100 ms since the meter was shown.
    int stutters = 0;
    std::optional<Core::FrameTimeHistory::Clock::time_point> counted_until;
    std::optional<Core::FrameTimeHistory::Clock::time_point> last_stutter;
    std::chrono::steady_clock::time_point shown_at{};
};

} // namespace Satronho
