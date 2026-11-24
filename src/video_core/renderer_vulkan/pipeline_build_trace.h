// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <functional>
#include <mutex>
#include <span>
#include <thread>

#include "common/common_types.h"

namespace Vulkan {

/// EDEN_NX2_PIPELINE_BUILD_TRACE=<file>: appends a line before and after every driver pipeline
/// build, flushed at once, so the last BEGIN without an END names the shaders a driver crashed on.
/// END lines carry the build's wall time (ms=), which includes time spent waiting on other builds.
/// Satronho NX: each session starts with a SESSION line (local date and time), every line carries
/// its second in the session (t=), and a file past 64 MiB moves to "<file>.old.txt" first.
inline void TracePipelineBuild(const char* phase, const char* kind, u64 key_hash,
                               std::span<const u64> stage_hashes, bool unoptimized,
                               double elapsed_ms = -1.0) {
    static const auto session_start = std::chrono::steady_clock::now();
    static std::FILE* const file = [] {
        const char* const path = std::getenv("EDEN_NX2_PIPELINE_BUILD_TRACE");
        if (path == nullptr || *path == '\0') {
            return static_cast<std::FILE*>(nullptr);
        }
        std::error_code ec;
        const std::filesystem::path trace{path};
        if (std::filesystem::file_size(trace, ec) > (u64{64} << 20) && !ec) {
            auto old = trace;
            old += ".old.txt";
            std::filesystem::rename(trace, old, ec);
        }
        std::FILE* const opened = std::fopen(path, "a");
        if (opened != nullptr) {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
#ifdef _WIN32
            const bool have_time = localtime_s(&local, &now) == 0;
#else
            const bool have_time = localtime_r(&now, &local) != nullptr;
#endif
            char when[32] = "?";
            if (have_time) {
                std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &local);
            }
            std::fprintf(opened, "SESSION %s\n", when);
            std::fflush(opened);
        }
        return opened;
    }();
    if (file == nullptr) {
        return;
    }
    static std::mutex mutex;
    std::scoped_lock lock{mutex};
    std::fprintf(file, "%s %s pipeline=%016llx thread=%zu%s", phase, kind,
                 static_cast<unsigned long long>(key_hash),
                 std::hash<std::thread::id>{}(std::this_thread::get_id()),
                 unoptimized ? " unoptimized" : "");
    for (size_t stage = 0; stage < stage_hashes.size(); ++stage) {
        if (stage_hashes[stage] != 0) {
            std::fprintf(file, " s%zu:%016llx", stage,
                         static_cast<unsigned long long>(stage_hashes[stage]));
        }
    }
    if (elapsed_ms >= 0.0) {
        std::fprintf(file, " ms=%.1f", elapsed_ms);
    }
    std::fprintf(file, " t=%.1f",
                 std::chrono::duration<double>(std::chrono::steady_clock::now() - session_start)
                     .count());
    std::fputc('\n', file);
    std::fflush(file);
}

} // namespace Vulkan
