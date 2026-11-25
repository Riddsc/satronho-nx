// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import org.json.JSONObject
import org.yuzu.yuzu_emu.NativeLibrary

/**
 * The game being played right now, watched once a second whether or not the meter is shown:
 * average FPS, stutters (and how many while shaders were being built) and the worst one. The
 * meter reads from here, and when the game closes it becomes the session summary and the line
 * on the game's card («60 fps · 2 tirones»), as the desktop's session summary does.
 */
object SesionActual {
    /** What one session left, kept per game and for the summary card. */
    data class Resumen(
        val programId: String,
        val title: String,
        val seconds: Long,
        val averageFps: Double,
        val lowFps: Double,
        val stutters: Int,
        val shaderStutters: Int,
        val worstMs: Int,
        val endedAt: Long,
        /** The battery's highest temperature in the session, °C (0 when unknown). */
        val maxTempC: Double = 0.0
    ) {
        fun toJson(): String = JSONObject()
            .put("programId", programId)
            .put("title", title)
            .put("seconds", seconds)
            .put("averageFps", averageFps)
            .put("lowFps", lowFps)
            .put("stutters", stutters)
            .put("shaderStutters", shaderStutters)
            .put("worstMs", worstMs)
            .put("endedAt", endedAt)
            .put("maxTempC", maxTempC)
            .toString()

        fun toJsonObject(): JSONObject = JSONObject(toJson())

        companion object {
            fun fromJson(text: String?): Resumen? {
                text ?: return null
                return try {
                    val json = JSONObject(text)
                    Resumen(
                        json.optString("programId"),
                        json.optString("title"),
                        json.optLong("seconds"),
                        json.optDouble("averageFps", 0.0),
                        json.optDouble("lowFps", 0.0),
                        json.optInt("stutters"),
                        json.optInt("shaderStutters"),
                        json.optInt("worstMs"),
                        json.optLong("endedAt"),
                        json.optDouble("maxTempC", 0.0)
                    )
                } catch (e: Exception) {
                    null
                }
            }
        }
    }

    private const val PENDING_KEY = "summary_pending"
    private const val LAST_KEY_PREFIX = "last_session_"
    private const val HISTORY_KEY_PREFIX = "history_"
    private const val TOTAL_KEY_PREFIX = "played_seconds_"
    private const val HISTORY_SIZE = 12

    /** From this battery temperature the phone is likely slowing itself down. */
    const val HOT_C = 42.0

    /** Sessions shorter than this don't leave a summary: opening a game and backing out. */
    private const val MIN_SECONDS = 30L

    private val handler = Handler(Looper.getMainLooper())
    private var running = false
    private var programId = ""
    private var title = ""
    private var startedAt = 0L

    private var fpsSum = 0.0
    private var fpsSamples = 0
    private val recentFps = ArrayList<Double>()

    var stutters = 0
        private set
    var shaderStutters = 0
        private set
    var worstMs = 0
        private set
    var lastStutterAt = 0L
        private set
    var lastShaderStutterAt = 0L
        private set
    var shadersBuilding = 0
        private set
    /** The battery's temperature now and the session's highest, °C (0 when unknown). */
    var tempC = 0.0
        private set
    var maxTempC = 0.0
        private set
    private var appContext: Context? = null
    private var sampleCount = 0

    private val tick = object : Runnable {
        override fun run() {
            if (!running) {
                return
            }
            sample()
            handler.postDelayed(this, 1000)
        }
    }

    /** Called when the game has really started (the loading screen is gone). */
    fun begin(context: Context, programId: String, title: String) {
        appContext = context.applicationContext
        if (running && programId == this.programId) {
            return
        }
        running = true
        this.programId = programId
        this.title = title
        startedAt = SystemClock.elapsedRealtime()
        fpsSum = 0.0
        fpsSamples = 0
        recentFps.clear()
        stutters = 0
        shaderStutters = 0
        worstMs = 0
        lastStutterAt = 0L
        lastShaderStutterAt = 0L
        shadersBuilding = 0
        tempC = 0.0
        maxTempC = 0.0
        sampleCount = 0
        readTemperature()
        // Drop whatever an earlier game left counted.
        NativeLibrary.satronhoTakeStutters()
        handler.removeCallbacks(tick)
        handler.postDelayed(tick, 1000)
    }

    private fun readTemperature() {
        val context = appContext ?: return
        try {
            val battery = context.registerReceiver(
                null, android.content.IntentFilter(android.content.Intent.ACTION_BATTERY_CHANGED)
            )
            val tenths = battery?.getIntExtra(android.os.BatteryManager.EXTRA_TEMPERATURE, 0) ?: 0
            if (tenths > 0) {
                tempC = tenths / 10.0
                maxTempC = maxOf(maxTempC, tempC)
            }
        } catch (e: Exception) {
            // No battery information on this device.
        }
    }

    private fun sample() {
        if (!NativeLibrary.isRunning() || NativeLibrary.isPaused()) {
            return
        }
        val now = SystemClock.elapsedRealtime()
        if (++sampleCount % 5 == 0) {
            readTemperature()
        }
        shadersBuilding = NativeLibrary.getShadersBuilding()
        val taken = NativeLibrary.satronhoTakeStutters()
        if (taken.size >= 2 && taken[0] > 0) {
            stutters += taken[0]
            worstMs = maxOf(worstMs, taken[1])
            lastStutterAt = now
            if (shadersBuilding > 0) {
                shaderStutters += taken[0]
                lastShaderStutterAt = now
            }
        }
        val stats = NativeLibrary.getPerfStats()
        val fps = if (stats.size > 1) stats[1] else 0.0
        if (fps > 0.5) {
            fpsSum += fps
            fpsSamples++
            recentFps.add(fps)
        }
    }

    /** Called when the game screen goes away. Leaves the summary for the games list. */
    fun end(context: Context) {
        if (!running) {
            return
        }
        running = false
        handler.removeCallbacks(tick)
        val seconds = (SystemClock.elapsedRealtime() - startedAt) / 1000
        if (seconds < MIN_SECONDS || fpsSamples == 0 || programId.isEmpty()) {
            return
        }
        // The low: the average of the slowest tenth of the seconds played.
        val sorted = recentFps.sorted()
        val tenth = maxOf(1, sorted.size / 10)
        val low = sorted.take(tenth).average()
        val resumen = Resumen(
            programId, title, seconds, fpsSum / fpsSamples, low, stutters, shaderStutters,
            worstMs, System.currentTimeMillis(), maxTempC
        )
        val prefs = Satronho.prefs(context)
        val history = org.json.JSONArray()
        history.put(resumen.toJsonObject())
        history(context, programId).take(HISTORY_SIZE - 1).forEach { history.put(it.toJsonObject()) }
        prefs.edit()
            .putString(LAST_KEY_PREFIX + programId, resumen.toJson())
            .putString(PENDING_KEY, resumen.toJson())
            .putString(HISTORY_KEY_PREFIX + programId, history.toString())
            .putLong(TOTAL_KEY_PREFIX + programId, totalSeconds(context, programId) + seconds)
            .apply()
        ContinuarWidget.refresh(context)
    }

    /** The session waiting to be shown on the games list, once. */
    fun takePending(context: Context): Resumen? {
        val prefs = Satronho.prefs(context)
        val pending = Resumen.fromJson(prefs.getString(PENDING_KEY, null))
        prefs.edit().remove(PENDING_KEY).apply()
        return pending
    }

    /** The game's last sessions, newest first. */
    fun history(context: Context, programId: String): List<Resumen> {
        val text = Satronho.prefs(context).getString(HISTORY_KEY_PREFIX + programId, null)
            ?: return listOfNotNull(last(context, programId))
        return try {
            val array = org.json.JSONArray(text)
            (0 until array.length()).mapNotNull { Resumen.fromJson(array.optJSONObject(it)?.toString()) }
        } catch (e: Exception) {
            emptyList()
        }
    }

    /** Everything played of this game in Satronho NX, seconds. */
    fun totalSeconds(context: Context, programId: String): Long =
        Satronho.prefs(context).getLong(TOTAL_KEY_PREFIX + programId, 0L)

    fun last(context: Context, programId: String): Resumen? =
        Resumen.fromJson(Satronho.prefs(context).getString(LAST_KEY_PREFIX + programId, null))
}
