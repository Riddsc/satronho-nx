// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

// Satronho NX test build: a sampling leak finder for operator new. About one allocation per
// 256 KiB allocated is recorded with its call stack; frees remove it again. The report lists the
// stacks holding the most memory that was never given back. Memory taken with malloc directly
// (the Vulkan driver, for one) is not seen, so comparing the total with malloc's own count says
// how much belongs to the driver.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include <cxxabi.h>
#include <dlfcn.h>
#include <jni.h>
#include <malloc.h>
#include <unwind.h>

#include "common/logging.h"

namespace {

constexpr std::int64_t SampleInterval = 256 * 1024;
constexpr std::size_t MaxFrames = 12;
constexpr std::size_t SkipFrames = 2;
constexpr std::size_t LiveSlots = std::size_t{1} << 19;
constexpr std::size_t StackSlots = std::size_t{1} << 14;
constexpr std::size_t MaxProbe = 64;

constexpr std::uintptr_t EmptyKey = 0;
constexpr std::uintptr_t DeletedKey = 1;

struct LiveEntry {
    std::atomic<std::uintptr_t> ptr{EmptyKey};
    std::atomic<std::uint32_t> stack{0};
    std::atomic<std::uint32_t> weight_kib{0};
};

struct StackEntry {
    std::atomic<std::uint64_t> hash{0};
    std::array<std::uintptr_t, MaxFrames> pcs{};
    std::atomic<std::uint32_t> depth{0};
    std::atomic<std::int64_t> live_bytes{0};
    std::atomic<std::int64_t> live_count{0};
    std::atomic<std::int64_t> total_bytes{0};
};

LiveEntry* g_live = nullptr;
StackEntry* g_stacks = nullptr;
std::atomic_bool g_ready{false};
std::atomic<std::int64_t> g_sampled_live{0};
std::atomic<std::int64_t> g_dropped{0};

thread_local std::int64_t t_until_sample = SampleInterval;
thread_local bool t_busy = false;

void EnsureTables() {
    static std::atomic_bool started{false};
    if (started.exchange(true)) {
        return;
    }
    // calloc'd pages stay untouched until used; neither table goes through operator new.
    g_live = static_cast<LiveEntry*>(std::calloc(LiveSlots, sizeof(LiveEntry)));
    g_stacks = static_cast<StackEntry*>(std::calloc(StackSlots, sizeof(StackEntry)));
    if (g_live != nullptr && g_stacks != nullptr) {
        g_ready.store(true, std::memory_order_release);
    }
}

std::size_t HashPtr(std::uintptr_t ptr) {
    std::uint64_t x = ptr;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    return static_cast<std::size_t>(x);
}

struct Frames {
    std::array<std::uintptr_t, MaxFrames + SkipFrames> pcs{};
    std::size_t count = 0;
};

_Unwind_Reason_Code Collect(_Unwind_Context* context, void* arg) {
    auto* frames = static_cast<Frames*>(arg);
    if (frames->count >= frames->pcs.size()) {
        return _URC_END_OF_STACK;
    }
    frames->pcs[frames->count++] = _Unwind_GetIP(context);
    return _URC_NO_REASON;
}

std::uint32_t FindOrAddStack(const Frames& frames) {
    std::uint64_t hash = 1469598103934665603ULL;
    const std::size_t first = (std::min)(SkipFrames, frames.count);
    for (std::size_t i = first; i < frames.count; ++i) {
        hash = (hash ^ frames.pcs[i]) * 1099511628211ULL;
    }
    if (hash < 2) {
        hash += 2;
    }
    std::size_t slot = static_cast<std::size_t>(hash) & (StackSlots - 1);
    for (std::size_t probe = 0; probe < MaxProbe; ++probe, slot = (slot + 1) & (StackSlots - 1)) {
        auto& entry = g_stacks[slot];
        std::uint64_t current = entry.hash.load(std::memory_order_acquire);
        if (current == hash) {
            return static_cast<std::uint32_t>(slot);
        }
        if (current == 0) {
            // 1 marks "being filled" until the frames are in.
            if (entry.hash.compare_exchange_strong(current, 1, std::memory_order_acq_rel)) {
                std::size_t depth = 0;
                for (std::size_t i = first; i < frames.count && depth < MaxFrames; ++i) {
                    entry.pcs[depth++] = frames.pcs[i];
                }
                entry.depth.store(static_cast<std::uint32_t>(depth), std::memory_order_relaxed);
                entry.hash.store(hash, std::memory_order_release);
                return static_cast<std::uint32_t>(slot);
            }
            if (current == hash) {
                return static_cast<std::uint32_t>(slot);
            }
        }
    }
    return UINT32_MAX;
}

void Record(void* ptr, std::size_t size) {
    const std::int64_t weight = (std::max)(static_cast<std::int64_t>(size), SampleInterval);
    Frames frames;
    _Unwind_Backtrace(Collect, &frames);
    const std::uint32_t stack = FindOrAddStack(frames);
    if (stack == UINT32_MAX) {
        g_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const auto key = reinterpret_cast<std::uintptr_t>(ptr);
    std::size_t slot = HashPtr(key) & (LiveSlots - 1);
    for (std::size_t probe = 0; probe < MaxProbe; ++probe, slot = (slot + 1) & (LiveSlots - 1)) {
        auto& entry = g_live[slot];
        std::uintptr_t current = entry.ptr.load(std::memory_order_relaxed);
        if (current != EmptyKey && current != DeletedKey) {
            continue;
        }
        if (entry.ptr.compare_exchange_strong(current, key, std::memory_order_acq_rel)) {
            entry.stack.store(stack, std::memory_order_relaxed);
            entry.weight_kib.store(static_cast<std::uint32_t>((weight + 1023) / 1024),
                                   std::memory_order_release);
            auto& s = g_stacks[stack];
            s.live_bytes.fetch_add(weight, std::memory_order_relaxed);
            s.live_count.fetch_add(1, std::memory_order_relaxed);
            s.total_bytes.fetch_add(weight, std::memory_order_relaxed);
            g_sampled_live.fetch_add(weight, std::memory_order_relaxed);
            return;
        }
    }
    g_dropped.fetch_add(1, std::memory_order_relaxed);
}

void Forget(void* ptr) {
    const auto key = reinterpret_cast<std::uintptr_t>(ptr);
    std::size_t slot = HashPtr(key) & (LiveSlots - 1);
    for (std::size_t probe = 0; probe < MaxProbe; ++probe, slot = (slot + 1) & (LiveSlots - 1)) {
        auto& entry = g_live[slot];
        const std::uintptr_t current = entry.ptr.load(std::memory_order_acquire);
        if (current == EmptyKey) {
            return;
        }
        if (current != key) {
            continue;
        }
        const std::uint32_t weight_kib = entry.weight_kib.load(std::memory_order_acquire);
        const std::uint32_t stack = entry.stack.load(std::memory_order_relaxed);
        std::uintptr_t expected = key;
        if (entry.ptr.compare_exchange_strong(expected, DeletedKey, std::memory_order_acq_rel)) {
            const std::int64_t weight = static_cast<std::int64_t>(weight_kib) * 1024;
            if (stack < StackSlots) {
                g_stacks[stack].live_bytes.fetch_sub(weight, std::memory_order_relaxed);
                g_stacks[stack].live_count.fetch_sub(1, std::memory_order_relaxed);
            }
            g_sampled_live.fetch_sub(weight, std::memory_order_relaxed);
        }
        return;
    }
}

void* AfterAlloc(void* ptr, std::size_t size) {
    if (ptr == nullptr || t_busy) {
        return ptr;
    }
    t_until_sample -= static_cast<std::int64_t>(size);
    if (t_until_sample > 0) {
        return ptr;
    }
    // A slightly varied interval keeps a loop of same-size allocations from always landing on
    // the same one.
    t_until_sample = SampleInterval - static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(ptr) & 0xFFF0);
    t_busy = true;
    EnsureTables();
    if (g_ready.load(std::memory_order_acquire)) {
        Record(ptr, size);
    }
    t_busy = false;
    return ptr;
}

void BeforeFree(void* ptr) {
    if (ptr == nullptr || !g_ready.load(std::memory_order_acquire)) {
        return;
    }
    Forget(ptr);
}

void* Allocate(std::size_t size) {
    void* ptr = std::malloc(size == 0 ? 1 : size);
    return AfterAlloc(ptr, size);
}

void* AllocateAligned(std::size_t size, std::align_val_t align) {
    std::size_t alignment = static_cast<std::size_t>(align);
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size == 0 ? 1 : size) != 0) {
        ptr = nullptr;
    }
    return AfterAlloc(ptr, size);
}

void Release(void* ptr) noexcept {
    BeforeFree(ptr);
    std::free(ptr);
}

std::string Describe(std::uintptr_t pc) {
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(pc), &info) == 0 || info.dli_fname == nullptr) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "0x%lx", static_cast<unsigned long>(pc));
        return buffer;
    }
    const char* lib = std::strrchr(info.dli_fname, '/');
    lib = lib != nullptr ? lib + 1 : info.dli_fname;
    char offset[32];
    std::snprintf(offset, sizeof(offset), "+0x%lx",
                  static_cast<unsigned long>(pc - reinterpret_cast<std::uintptr_t>(info.dli_fbase)));
    std::string out = std::string{lib} + offset;
    if (info.dli_sname != nullptr) {
        int status = 0;
        char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
        std::string name = (status == 0 && demangled != nullptr) ? demangled : info.dli_sname;
        std::free(demangled);
        if (name.size() > 140) {
            name.resize(140);
        }
        out += " " + name;
    }
    return out;
}

} // Anonymous namespace

void* operator new(std::size_t size) {
    if (void* ptr = Allocate(size)) {
        return ptr;
    }
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) {
    if (void* ptr = Allocate(size)) {
        return ptr;
    }
    throw std::bad_alloc{};
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return Allocate(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return Allocate(size);
}
void* operator new(std::size_t size, std::align_val_t align) {
    if (void* ptr = AllocateAligned(size, align)) {
        return ptr;
    }
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size, std::align_val_t align) {
    if (void* ptr = AllocateAligned(size, align)) {
        return ptr;
    }
    throw std::bad_alloc{};
}
void* operator new(std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    return AllocateAligned(size, align);
}
void* operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    return AllocateAligned(size, align);
}
void operator delete(void* ptr) noexcept {
    Release(ptr);
}
void operator delete[](void* ptr) noexcept {
    Release(ptr);
}
void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    Release(ptr);
}
void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    Release(ptr);
}
void operator delete(void* ptr, std::size_t) noexcept {
    Release(ptr);
}
void operator delete[](void* ptr, std::size_t) noexcept {
    Release(ptr);
}
void operator delete(void* ptr, std::align_val_t) noexcept {
    Release(ptr);
}
void operator delete[](void* ptr, std::align_val_t) noexcept {
    Release(ptr);
}
void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept {
    Release(ptr);
}
void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept {
    Release(ptr);
}
void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept {
    Release(ptr);
}
void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept {
    Release(ptr);
}

extern "C" JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_satronhoLeakReport(JNIEnv* env, jobject obj) {
    if (!g_ready.load(std::memory_order_acquire)) {
        return;
    }
    t_busy = true;
    struct Top {
        std::uint32_t slot;
        std::int64_t live;
        std::int64_t count;
    };
    std::vector<Top> tops;
    for (std::uint32_t i = 0; i < StackSlots; ++i) {
        const auto& entry = g_stacks[i];
        if (entry.hash.load(std::memory_order_acquire) < 2) {
            continue;
        }
        const std::int64_t live = entry.live_bytes.load(std::memory_order_relaxed);
        if (live >= (std::int64_t{8} << 20)) {
            tops.push_back({i, live, entry.live_count.load(std::memory_order_relaxed)});
        }
    }
    std::sort(tops.begin(), tops.end(), [](const Top& a, const Top& b) { return a.live > b.live; });
    const struct mallinfo info = mallinfo();
    LOG_INFO(Frontend,
             "Satronho leaks: operator new holds about {} MiB (malloc in use {} MiB), {} samples "
             "dropped",
             g_sampled_live.load(std::memory_order_relaxed) >> 20,
             static_cast<std::uint64_t>(info.uordblks) >> 20,
             g_dropped.load(std::memory_order_relaxed));
    const std::size_t shown = (std::min)(tops.size(), std::size_t{6});
    for (std::size_t n = 0; n < shown; ++n) {
        const auto& entry = g_stacks[tops[n].slot];
        std::string frames;
        const std::uint32_t depth = entry.depth.load(std::memory_order_relaxed);
        for (std::uint32_t i = 0; i < depth; ++i) {
            frames += (i == 0 ? "" : " <- ") + Describe(entry.pcs[i]);
        }
        LOG_INFO(Frontend, "Satronho leaks #{}: {} MiB live in {} samples: {}", n + 1,
                 tops[n].live >> 20, tops[n].count, frames);
    }
    t_busy = false;
}
