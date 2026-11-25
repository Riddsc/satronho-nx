// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/satronho/session.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include <QCoreApplication>

#include "common/logging.h"
#include "core/core.h"
#include "core/perf_stats.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/qt_common.h"
#include "video_core/gpu.h"
#include "video_core/shader_notify.h"
#include "yuzu/satronho/directo.h"
#include "yuzu/satronho/juegos.h"

namespace Satronho::Session {

namespace {

using Clock = Core::FrameTimeHistory::Clock;

QString Tr(const char* text) {
    return QCoreApplication::translate("Satronho::Session", text);
}

constexpr double StutterMs = 100.0;

struct State {
    bool running{};
    u64 program_id{};
    QString game;
    std::chrono::steady_clock::time_point begin;
    std::vector<float> fps;
    std::optional<Clock::time_point> counted_until;
    int stutters{};
    int shader_stutters{};
    int shaders_at_begin{};
    bool stream_mode{};
};

State& Current() {
    static State state;
    return state;
}

VideoCore::ShaderNotify* Notify() {
    if (QtCommon::system == nullptr || !QtCommon::system->IsPoweredOn()) {
        return nullptr;
    }
    return &QtCommon::system->GPU().ShaderNotify();
}

double Percentile(std::vector<float> values, double fraction) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const auto index = static_cast<size_t>(fraction * static_cast<double>(values.size() - 1));
    return values[index];
}

} // namespace

bool Enabled() {
    return UISettings::values.satronho_session_summary.GetValue();
}

void SetEnabled(bool enabled) {
    UISettings::values.satronho_session_summary.SetValue(enabled);
}

bool WantsFrameTimes() {
    return Enabled() && Current().running;
}

void Begin(u64 program_id, const QString& game) {
    State& state = Current();
    state = {};
    state.running = true;
    state.program_id = program_id;
    state.game = game;
    state.begin = std::chrono::steady_clock::now();
    if (const auto* notify = Notify()) {
        state.shaders_at_begin = notify->TotalComplete();
    }
}

void Tick(double fps) {
    State& state = Current();
    if (!state.running || QtCommon::system == nullptr) {
        return;
    }
    if (!std::isnan(fps) && fps > 0.0) {
        state.fps.push_back(static_cast<float>(fps));
    }
    state.stream_mode = state.stream_mode || Satronho::Directo::StreamModeOn();
    if (!Enabled()) {
        return;
    }
    auto& perf_stats = QtCommon::system->GetPerfStats();
    perf_stats.SetFrameTimeHistoryEnabled(true);
    const auto* notify = Notify();
    const bool building = notify != nullptr && notify->ShadersLeft() > 0;
    const auto frames = perf_stats.GetRecentFrameTimes(std::chrono::seconds{3});
    for (const auto& frame : frames) {
        if (state.counted_until && frame.end <= *state.counted_until) {
            continue;
        }
        if (frame.ms > StutterMs) {
            ++state.stutters;
            if (building) {
                ++state.shader_stutters;
            }
        }
    }
    if (!frames.empty()) {
        state.counted_until = frames.back().end;
    }
}

std::optional<Summary> End() {
    State& state = Current();
    if (!state.running) {
        return std::nullopt;
    }
    state.running = false;
    Summary summary{
        .game = state.game,
        .program_id = state.program_id,
        .minutes = std::chrono::duration<double>(std::chrono::steady_clock::now() - state.begin)
                       .count() /
                   60.0,
        .typical_fps = Percentile(state.fps, 0.5),
        .slow_fps = Percentile(state.fps, 0.1),
        .stutters = state.stutters,
        .shader_stutters = state.shader_stutters,
        .stream_mode = state.stream_mode,
    };
    if (const auto* notify = Notify()) {
        summary.shaders_built = std::max(notify->TotalComplete() - state.shaders_at_begin, 0);
    }
    LOG_INFO(Frontend,
             "Satronho session summary: {:016X} {:.1f} min, {:.1f} typical fps, {:.1f} slowest "
             "10%, {} stutters over 100 ms ({} while shaders built), {} shaders built{}",
             summary.program_id, summary.minutes, summary.typical_fps, summary.slow_fps,
             summary.stutters, summary.shader_stutters, summary.shaders_built,
             summary.stream_mode ? ", stream mode" : "");
    Juegos::AddSession(summary);
    if (summary.minutes < 1.0) {
        return std::nullopt;
    }
    return summary;
}

QString ShortText(const Summary& summary) {
    QString text = Tr("<b>%1</b>: %2 min jugando, %3 FPS típicos.")
                       .arg(summary.game.toHtmlEscaped())
                       .arg(std::round(summary.minutes), 0, 'f', 0)
                       .arg(summary.typical_fps, 0, 'f', 0);
    if (summary.stutters == 0) {
        text += Tr(" Sin tirones.");
    } else if (summary.shader_stutters > 0) {
        text += Tr(" %1 tirones, %2 mientras se armaban shaders.")
                    .arg(summary.stutters)
                    .arg(summary.shader_stutters);
    } else {
        text += Tr(" %1 tirones.").arg(summary.stutters);
    }
    if (summary.stream_mode) {
        text += Tr(" Con modo directo.");
    }
    return text;
}

QString LongText(const Summary& summary) {
    QString text = QStringLiteral("<h3>%1</h3>").arg(summary.game.toHtmlEscaped());
    text += QStringLiteral("<table cellspacing=\"6\">");
    const auto row = [&text](const QString& name, const QString& value) {
        text += QStringLiteral("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(name, value);
    };
    row(Tr("Tiempo con el juego abierto"), Tr("%1 min").arg(summary.minutes, 0, 'f', 1));
    row(Tr("FPS típicos"), QString::number(summary.typical_fps, 'f', 1));
    row(Tr("El 10 % más lento"), QString::number(summary.slow_fps, 'f', 1));
    row(Tr("Tirones (más de 100 ms)"), QString::number(summary.stutters));
    row(Tr("…mientras se armaban shaders"), QString::number(summary.shader_stutters));
    row(Tr("Shaders armados"), QString::number(summary.shaders_built));
    if (summary.stream_mode) {
        row(Tr("Modo directo"), Tr("activo en esta sesión"));
    }
    text += QStringLiteral("</table>");
    text += Tr("<p>Los FPS cuentan todo el tiempo con el juego abierto, menús y pausas incluidos. "
               "Si muchos tirones salen mientras se armaban shaders, «Preparar para directo» "
               "los arma antes de jugar.</p><p>El mismo resumen queda en el log de la sesión "
               "(carpeta log\\sesiones).</p>");
    return text;
}

} // namespace Satronho::Session
