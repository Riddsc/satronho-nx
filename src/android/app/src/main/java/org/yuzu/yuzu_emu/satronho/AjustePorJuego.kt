// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.content.Context
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.utils.NativeConfig

/**
 * Ñoño tunes one game: when a game stays slow for two sessions in a row, he offers to lower its
 * resolution one step in that game's own settings, without touching the other games.
 */
object AjustePorJuego {
    /** Below this average a session counts as slow (30 fps games average about 30). */
    private const val SLOW_FPS = 25.0
    private const val SLOW_SESSIONS = 2

    private fun handledKey(programId: String) = "nono_handled_$programId"

    /** The two newest sessions were slow and Ñoño hasn't already spoken about them. */
    fun shouldSuggest(context: Context, programId: String): Boolean {
        val history = SesionActual.history(context, programId)
        if (history.size < SLOW_SESSIONS) {
            return false
        }
        val recent = history.take(SLOW_SESSIONS)
        if (recent.any { it.averageFps >= SLOW_FPS }) {
            return false
        }
        // Wait for two new slow sessions after each suggestion.
        val handledAt = Satronho.prefs(context).getLong(handledKey(programId), 0L)
        return recent.all { it.endedAt > handledAt }
    }

    fun markHandled(context: Context, programId: String) {
        Satronho.prefs(context).edit()
            .putLong(handledKey(programId), System.currentTimeMillis()).apply()
    }

    /** The resolution names, «1/2X (360p/540p)» and so on, by setting value. */
    private fun resolutionName(context: Context, value: Int): String =
        context.resources.getStringArray(R.array.rendererResolutionNames).getOrNull(value)
            ?: value.toString()

    /**
     * Lowers the game's own resolution one step. Returns what changed, to tell the user, or null
     * when it is already at the lowest.
     */
    fun lowerResolution(context: Context, game: Game): String? {
        return try {
            SettingsFile.loadCustomConfig(game)
            val current = IntSetting.RENDERER_RESOLUTION.getInt(false)
            if (current <= 0) {
                NativeConfig.unloadPerGameConfig()
                return null
            }
            IntSetting.RENDERER_RESOLUTION.setInt(current - 1)
            NativeConfig.savePerGameConfig()
            NativeConfig.unloadPerGameConfig()
            markHandled(context, game.programId)
            context.getString(
                R.string.satronho_nono_tuned,
                game.title,
                resolutionName(context, current),
                resolutionName(context, current - 1)
            )
        } catch (e: Exception) {
            try {
                NativeConfig.unloadPerGameConfig()
            } catch (ignored: Exception) {
            }
            null
        }
    }
}
