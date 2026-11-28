// SPDX-FileCopyrightText: Copyright 2026 Satronho
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.ActivityManager
import android.content.ComponentCallbacks2
import android.content.Context
import android.os.Build
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import org.yuzu.yuzu_emu.utils.Log

/**
 * Notes in the log what Android sees of memory: why the previous session ended (an LMK kill
 * leaves nothing in our own log), the process and phone memory every 10 s while a game runs,
 * and each low-memory warning Android sends.
 */
object MemoryWatch {
    private const val PERIOD_MS = 10_000L

    @Volatile
    private var thread: Thread? = null

    fun logLastExit(context: Context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return
        }
        try {
            val am = context.getSystemService(ActivityManager::class.java) ?: return
            val exits = am.getHistoricalProcessExitReasons(context.packageName, 0, 3)
            if (exits.isEmpty()) {
                Log.info("Satronho memory: no previous exit recorded")
                return
            }
            val format = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US)
            exits.forEachIndexed { index, exit ->
                Log.info(
                    "Satronho memory: previous exit #${index + 1} at " +
                        "${format.format(Date(exit.timestamp))}: reason=${reasonName(exit.reason)} " +
                        "status=${exit.status} importance=${exit.importance} " +
                        "pss_mib=${exit.pss / 1024} rss_mib=${exit.rss / 1024} " +
                        "description=${exit.description ?: "-"}"
                )
            }
        } catch (e: Exception) {
            Log.warning("Satronho memory: could not read the previous exit: ${e.message}")
        }
    }

    /** What the last abort left in the cache folder (thread and stack), into this log. */
    fun logLastAbort(context: Context) {
        try {
            val file = File(context.cacheDir, "satronho_abort.txt")
            if (!file.exists()) {
                return
            }
            val text = file.readText().trim()
            if (text.isNotEmpty()) {
                text.lines().forEach { Log.warning("Satronho last abort: $it") }
            }
            file.delete()
        } catch (e: Exception) {
            Log.warning("Satronho memory: could not read the last abort: ${e.message}")
        }
    }

    fun start(context: Context) {
        stop()
        val am = context.applicationContext.getSystemService(ActivityManager::class.java)
        val worker = Thread({
            try {
                while (!Thread.currentThread().isInterrupted) {
                    val before = android.os.Debug.getNativeHeapSize()
                    try {
                        org.yuzu.yuzu_emu.NativeLibrary.satronhoPurgeNativeMemory()
                    } catch (e: Throwable) {
                    }
                    val after = android.os.Debug.getNativeHeapSize()
                    if (before - after >= 16L * 1024 * 1024) {
                        Log.info("Satronho memory: purge gave back ${(before - after) / (1024 * 1024)} MiB")
                    }
                    logSnapshot(am, "periodic")
                    logMappings()
                    try {
                        org.yuzu.yuzu_emu.NativeLibrary.satronhoLeakReport()
                    } catch (e: Throwable) {
                    }
                    Thread.sleep(PERIOD_MS)
                }
            } catch (e: InterruptedException) {
            }
        }, "SatronhoMemWatch")
        worker.isDaemon = true
        thread = worker
        worker.start()
    }

    fun stop() {
        thread?.interrupt()
        thread = null
    }

    fun onTrimMemory(context: Context, level: Int) {
        val am = context.applicationContext.getSystemService(ActivityManager::class.java)
        Log.warning("Satronho memory: Android low-memory warning level=${trimName(level)}")
        logSnapshot(am, "trim")
    }

    private fun logSnapshot(am: ActivityManager?, why: String) {
        val status = readKb(File("/proc/self/status"), "VmRSS:", "VmSwap:")
        val meminfo = readKb(File("/proc/meminfo"), "MemAvailable:", "SwapFree:", "MemTotal:")
        var low = "?"
        var threshold = -1L
        if (am != null) {
            val info = ActivityManager.MemoryInfo()
            am.getMemoryInfo(info)
            low = info.lowMemory.toString()
            threshold = info.threshold / (1024 * 1024)
        }
        val guest = try {
            org.yuzu.yuzu_emu.NativeLibrary.satronhoGuestMemoryMib()
        } catch (e: Throwable) {
            -1L
        }
        val heapUsed = android.os.Debug.getNativeHeapAllocatedSize() / (1024 * 1024)
        val heapSize = android.os.Debug.getNativeHeapSize() / (1024 * 1024)
        Log.info(
            "Satronho memory ($why): rss_mib=${mib(status["VmRSS:"])} game_mib=$guest " +
                "malloc_used_mib=$heapUsed malloc_held_mib=$heapSize " +
                "swap_mib=${mib(status["VmSwap:"])} " +
                "phone_available_mib=${mib(meminfo["MemAvailable:"])} " +
                "phone_swap_free_mib=${mib(meminfo["SwapFree:"])} " +
                "phone_total_mib=${mib(meminfo["MemTotal:"])} " +
                "low_memory=$low threshold_mib=$threshold"
        )
    }

    /** The largest kinds of memory of the process by name (from /proc/self/smaps), in MiB. */
    private fun logMappings() {
        val totals = HashMap<String, LongArray>()
        try {
            var name = "?"
            File("/proc/self/smaps").forEachLine { line ->
                val first = line.firstOrNull() ?: return@forEachLine
                if (first.isDigit() || (first in 'a'..'f')) {
                    // Header: address perms offset dev inode [path]
                    val parts = line.split(Regex("\\s+"), limit = 6)
                    name = if (parts.size >= 6 && parts[5].isNotBlank()) {
                        groupName(parts[5].trim())
                    } else {
                        "[anon]"
                    }
                } else if (line.startsWith("Rss:")) {
                    val kb = line.substring(4).trim().split(' ').firstOrNull()?.toLongOrNull() ?: 0L
                    val entry = totals.getOrPut(name) { LongArray(2) }
                    entry[0] += kb
                } else if (line.startsWith("Private_Dirty:")) {
                    val kb = line.substring(14).trim().split(' ').firstOrNull()?.toLongOrNull() ?: 0L
                    totals.getOrPut(name) { LongArray(2) }[1] += kb
                }
            }
        } catch (e: Exception) {
            Log.warning("Satronho memory: could not read smaps: ${e.message}")
            return
        }
        val top = totals.entries.sortedByDescending { it.value[0] }.take(10)
        Log.info(
            "Satronho memory map: " + top.joinToString("; ") {
                "${it.key} rss ${it.value[0] / 1024} dirty ${it.value[1] / 1024}"
            }
        )
    }

    private fun groupName(path: String): String {
        // Keep the kind, not the instance: "[anon:scudo:primary]", "/dev/mali0", a library name.
        if (path.startsWith("[")) {
            return path.replace(Regex("\\d+"), "#")
        }
        return path.substringAfterLast('/').replace(" (deleted)", "")
    }

    private fun mib(kb: Long?): String = if (kb == null) "?" else (kb / 1024).toString()

    private fun readKb(file: File, vararg keys: String): Map<String, Long> {
        val out = HashMap<String, Long>()
        try {
            file.forEachLine { line ->
                for (key in keys) {
                    if (line.startsWith(key)) {
                        line.substring(key.length).trim().split(' ').firstOrNull()
                            ?.toLongOrNull()?.let { out[key] = it }
                    }
                }
            }
        } catch (e: Exception) {
        }
        return out
    }

    private fun reasonName(reason: Int): String = when (reason) {
        1 -> "EXIT_SELF"
        2 -> "SIGNALED"
        3 -> "LOW_MEMORY"
        4 -> "CRASH"
        5 -> "CRASH_NATIVE"
        6 -> "ANR"
        7 -> "INITIALIZATION_FAILURE"
        8 -> "PERMISSION_CHANGE"
        9 -> "EXCESSIVE_RESOURCE_USAGE"
        10 -> "USER_REQUESTED"
        11 -> "USER_STOPPED"
        12 -> "DEPENDENCY_DIED"
        13 -> "OTHER"
        14 -> "FREEZER"
        15 -> "PACKAGE_STATE_CHANGE"
        16 -> "PACKAGE_UPDATED"
        else -> "UNKNOWN($reason)"
    }

    @Suppress("DEPRECATION")
    private fun trimName(level: Int): String = when (level) {
        ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE -> "RUNNING_MODERATE"
        ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW -> "RUNNING_LOW"
        ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL -> "RUNNING_CRITICAL"
        ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN -> "UI_HIDDEN"
        ComponentCallbacks2.TRIM_MEMORY_BACKGROUND -> "BACKGROUND"
        ComponentCallbacks2.TRIM_MEMORY_MODERATE -> "MODERATE"
        ComponentCallbacks2.TRIM_MEMORY_COMPLETE -> "COMPLETE"
        else -> level.toString()
    }
}
