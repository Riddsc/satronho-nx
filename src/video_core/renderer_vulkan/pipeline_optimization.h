// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string_view>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan.h"

namespace Vulkan {

/// Satronho: AMD's own driver takes 8-26 s (up to a minute) to optimize some compute shaders,
/// which made a boot with a lost cache last ~40 min. Longshot builds every pipeline on that
/// driver without its optimizer by default and tested it that way (RX_VK_NOOPT, 1.0.0).
/// EDEN_NX2_AMD_NOOPT=0 restores the optimizer.
inline bool AmdBuildsUnoptimized() {
    static const bool amd_noopt = [] {
        const char* const value = std::getenv("EDEN_NX2_AMD_NOOPT");
        return value == nullptr || std::string_view{value} != "0";
    }();
    return amd_noopt;
}

inline bool BuildsUnoptimized(VkDriverId driver_id, u64 shader_hash,
                              bool amd_unoptimized = AmdBuildsUnoptimized()) {
    if (driver_id == VK_DRIVER_ID_AMD_PROPRIETARY && amd_unoptimized) {
        return true;
    }
    // The Legend of Zelda: Ocarina of Time (0400486024DEC000), driver 2.0.353 on an RX 6600 XT
    static constexpr std::array<u64, 2> amd_proprietary{
        0xeff3ccbe1438679dULL, // compute shader built as the AV1 intro starts: 6 min (#200)
        0xf9ea14a22394f5b1ULL, // compute shader built as the AV1 intro starts: 18 min (#200)
    };
    return driver_id == VK_DRIVER_ID_AMD_PROPRIETARY &&
           std::ranges::find(amd_proprietary, shader_hash) != amd_proprietary.end();
}

} // namespace Vulkan
