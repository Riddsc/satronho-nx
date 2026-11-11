// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <deque>
#include <optional>
#include <vector>
#include <QWidget>

#include "common/dkb_cpu_trace.h"
#include "core/perf_stats.h"
#include "video_core/renderer_base.h"

namespace VideoCore {
class ShaderNotify;
}

class MainWindow;

class PerformanceOverlay : public QWidget {
    Q_OBJECT

public:
    explicit PerformanceOverlay(MainWindow* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void resetPosition(const QPoint& pos);
    void updateStats(const Core::PerfStatsResults& results, const VideoCore::ShaderNotify& shaders);
    void paintGraph(QPainter& painter, const QRectF& area);
    /// Satronho NX's row: the emulator's RAM and its video memory against the driver's budget.
    void paintMemory(QPainter& painter, const QFont& font);

    MainWindow* m_mainWindow = nullptr;

    QPoint m_offset{25, 75};

    // drag
    QPoint m_drag_start_pos;

    std::optional<double> m_fps;
    /// The last 5 s of 500 ms fps readings, for min/median/max
    std::deque<float> m_fpsSamples;
    std::optional<double> m_frameMs;
    std::optional<double> m_cpuPercent;
    std::optional<double> m_gpuPercent;
    std::optional<Common::DkbCpuTrace::Snapshot> m_cpuSnapshot;
    std::vector<Core::FrameTimeHistory::Entry> m_frames;
    QString m_tags;
    std::optional<u64> m_ramBytes;
    std::optional<VideoCore::RendererBase::VideoMemoryUsage> m_videoMemory;

signals:
    void closed();
};
