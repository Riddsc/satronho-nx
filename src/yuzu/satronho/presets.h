// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <vector>

#include <QString>
#include <QStringList>

#include "common/common_types.h"

namespace VkDeviceInfo {
class Record;
}

namespace Satronho {

/// What the suggestions are based on.
struct Hardware {
    QString gpu_name;        ///< Empty when Vulkan found no GPU
    u32 vendor_id = 0;
    u64 vram_bytes = 0;
    QString cpu_name;
    unsigned cpu_threads = 0;
    u64 ram_bytes = 0;
};

/// A family of graphics cards with what it gets.
enum class GpuFamily {
    Unknown,
    AmdRdna4,    // Radeon RX 9000
    AmdRdna3,    // Radeon RX 7000 and Radeon 700M integrated
    AmdRdna2,    // Radeon RX 6000
    AmdOlder,    // RX 5000, Vega, Polaris
    NvBlackwell, // GeForce RTX 50
    NvAda,       // GeForce RTX 40
    NvAmpere,    // GeForce RTX 30
    NvTuring,    // GeForce RTX 20, GTX 16
    NvOlder,     // GTX 10 and older
    IntelArc,    // Arc A and B
    IntelIntegrated,
};

/// One setting the suggestion changes.
struct SettingChange {
    QString label;      ///< As the configuration window names it
    QString location;   ///< Where it is ("Graphics", "Graphics → Advanced")
    QString current;    ///< Current value, as text
    QString suggested;  ///< Suggested value, as text
    QString reason;     ///< Why, in plain words
    bool differs = false;
    std::function<void()> apply;  ///< Sets the suggested global value
    const char* ini_key = nullptr;      ///< Key in the per-game files
    const char* ini_section = nullptr;  ///< Section in the per-game files
};

struct Suggestion {
    GpuFamily family = GpuFamily::Unknown;
    QString family_name;           ///< "AMD Radeon RX 6000 (RDNA 2)"
    QString summary;               ///< One paragraph for this PC
    QStringList notes;             ///< Processor, RAM and card notes
    std::vector<SettingChange> changes;
};

/// The hardware of this PC, with the GPU Vulkan will use (the configured device).
Hardware DetectHardware(const std::vector<VkDeviceInfo::Record>& records);

/// Which family a card belongs to.
GpuFamily ClassifyGpu(const QString& name, u32 vendor_id);

/// The suggested settings for this hardware, compared with the current ones.
Suggestion Suggest(const Hardware& hardware);

/// The generation guide: what each family gets, as rich text.
QString GenerationGuideHtml();

/// Applies every change that differs. With `per_game`, games with their own value for one of
/// those settings are switched to the global one. Returns how many game files changed.
int ApplySuggestion(const Suggestion& suggestion, bool per_game);

} // namespace Satronho
