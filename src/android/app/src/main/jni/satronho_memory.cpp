// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include <dlfcn.h>
#include <malloc.h>
#include <fcntl.h>
#include <jni.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <unwind.h>

#include "common/android/android_common.h"
#include "common/logging.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "native.h"

namespace Core {
void SatronhoJitTotals(u64& blocks_compiled, u64& cache_clears);
}

// Satronho NX: memory options for Switch 2 games on phones, and a note of why the emulator
// aborted (Android's own exit reason only says "signal 6").
namespace {

int g_abort_fd = -1;
std::atomic_bool g_handlers_installed{false};
struct sigaction g_previous_abort {};

void WriteText(const char* text) {
    if (g_abort_fd >= 0 && text != nullptr) {
        [[maybe_unused]] const auto written = write(g_abort_fd, text, std::strlen(text));
    }
}

void WriteHex(uintptr_t value) {
    char buffer[2 + 16 + 1];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        const unsigned digit = (value >> ((15 - i) * 4)) & 0xF;
        buffer[2 + i] = static_cast<char>(digit < 10 ? '0' + digit : 'a' + digit - 10);
    }
    buffer[18] = '\0';
    WriteText(buffer);
}

struct Frames {
    uintptr_t pcs[24];
    int count;
};

_Unwind_Reason_Code CollectFrame(_Unwind_Context* context, void* arg) {
    auto* frames = static_cast<Frames*>(arg);
    if (frames->count >= 24) {
        return _URC_END_OF_STACK;
    }
    frames->pcs[frames->count++] = _Unwind_GetIP(context);
    return _URC_NO_REASON;
}

void WriteBacktrace() {
    Frames frames{};
    _Unwind_Backtrace(CollectFrame, &frames);
    for (int i = 0; i < frames.count; ++i) {
        Dl_info info{};
        WriteText("  ");
        if (dladdr(reinterpret_cast<void*>(frames.pcs[i]), &info) != 0 && info.dli_fname) {
            const char* name = std::strrchr(info.dli_fname, '/');
            WriteText(name != nullptr ? name + 1 : info.dli_fname);
            WriteText("+");
            WriteHex(frames.pcs[i] - reinterpret_cast<uintptr_t>(info.dli_fbase));
            if (info.dli_sname != nullptr) {
                WriteText(" ");
                WriteText(info.dli_sname);
            }
        } else {
            WriteHex(frames.pcs[i]);
        }
        WriteText("\n");
    }
}

void OnAbort(int signal, siginfo_t* info, void* context) {
    char thread[17]{};
    prctl(PR_GET_NAME, thread, 0, 0, 0);
    WriteText("Satronho: the emulator aborted (signal 6) on thread ");
    WriteText(thread);
    WriteText("\n");
    WriteBacktrace();
    if (g_abort_fd >= 0) {
        fsync(g_abort_fd);
    }
    sigaction(SIGABRT, &g_previous_abort, nullptr);
    if ((g_previous_abort.sa_flags & SA_SIGINFO) != 0 && g_previous_abort.sa_sigaction != nullptr) {
        g_previous_abort.sa_sigaction(signal, info, context);
    } else if (g_previous_abort.sa_handler != SIG_DFL && g_previous_abort.sa_handler != SIG_IGN &&
               g_previous_abort.sa_handler != nullptr) {
        g_previous_abort.sa_handler(signal);
    }
    raise(SIGABRT);
}

[[noreturn]] void OnTerminate() {
    std::string what = "unknown";
    if (const auto current = std::current_exception()) {
        try {
            std::rethrow_exception(current);
        } catch (const std::exception& e) {
            what = e.what();
        } catch (...) {
            what = "a non-standard exception";
        }
    } else {
        what = "no exception (std::terminate called directly)";
    }
    WriteText("Satronho: std::terminate: ");
    WriteText(what.c_str());
    WriteText("\n");
    LOG_CRITICAL(Frontend, "Satronho: std::terminate: {}", what);
    std::abort();
}

void InstallHandlers(const std::string& abort_path) {
    if (g_abort_fd < 0 && !abort_path.empty()) {
        g_abort_fd = open(abort_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    }
    if (g_handlers_installed.exchange(true)) {
        return;
    }
    std::set_terminate(OnTerminate);
    struct sigaction action {};
    action.sa_sigaction = OnAbort;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    sigaction(SIGABRT, &action, &g_previous_abort);
}

void SetOrClear(const char* name, bool on, const char* value) {
    if (on) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
}

} // Anonymous namespace

extern "C" {

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_satronhoSetMemoryOptions(
    JNIEnv* env, jobject obj, jboolean trim_zero, jboolean smaller_pool, jboolean file_backed,
    jstring j_cache_dir) {
    const std::string cache_dir = Common::Android::GetJString(env, j_cache_dir);
    SetOrClear("SATRONHO_TRIM_ZERO", trim_zero, "1");
    SetOrClear("SATRONHO_NX2_POOL_MIB", smaller_pool, "7168");
    SetOrClear("SATRONHO_GUEST_FILE_DIR", file_backed && !cache_dir.empty(), cache_dir.c_str());
    // What the texture and buffer caches and the GPU heaps hold, every 2 s.
    setenv("EDEN_NX2_MEM_TRACE", "1", 1);
    // Satronho NX test: draws per frame every 10 s («Draw reuse per frame»), to tell a game that
    // draws a black picture from one that draws nothing.
    setenv("EDEN_NX2_DRAW_REUSE_PROBE", "1", 1);
    InstallHandlers(cache_dir.empty() ? std::string{} : cache_dir + "/satronho_abort.txt");
    LOG_INFO(Frontend,
             "Satronho memory options: free zero pages {}, smaller game memory {}, game memory in "
             "a file {}",
             trim_zero ? "on" : "off", smaller_pool ? "on" : "off", file_backed ? "on" : "off");
}

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_satronhoPurgeNativeMemory(JNIEnv* env,
                                                                                    jobject obj) {
    // Freed memory the allocator still holds goes back to Android.
#ifdef M_PURGE
    mallopt(M_PURGE, 0);
#endif
    // How much the CPU translator works: many blocks a second and frequent clears mean it keeps
    // translating the same code again.
    static u64 last_compiled = 0;
    static u64 last_clears = 0;
    u64 compiled = 0;
    u64 clears = 0;
    Core::SatronhoJitTotals(compiled, clears);
    if (compiled != last_compiled || clears != last_clears) {
        LOG_INFO(Frontend, "Satronho JIT: {} blocks compiled and {} full cache clears since the last "
                 "line ({} and {} in total)",
                 compiled - last_compiled, clears - last_clears, compiled, clears);
    }
    last_compiled = compiled;
    last_clears = clears;
}

JNIEXPORT jlong JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_satronhoGuestMemoryMib(JNIEnv* env,
                                                                                     jobject obj) {
    auto& system = EmulationSession::GetInstance().System();
    if (!system.IsPoweredOn()) {
        return -1;
    }
    const auto* process = system.ApplicationProcess();
    if (process == nullptr) {
        return -1;
    }
    return static_cast<jlong>(process->GetUsedUserPhysicalMemorySize(system.Kernel()) >> 20);
}

} // extern "C"
