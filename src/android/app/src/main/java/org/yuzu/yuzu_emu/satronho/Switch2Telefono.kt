// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.utils.NativeConfig

/**
 * The first time a Switch 2 game opens on the phone, Troya offers a phone profile for that game
 * only: a lighter internal resolution with FSR to sharpen it, and asynchronous shaders, so it
 * starts as smooth as the phone allows. Phones under 16 GB of RAM also hear that the game may
 * close when Android runs short of memory. Asked once per game; the game's properties keep it.
 */
object Switch2Telefono {
    /** Switch 2 titles ask for 16 GB of host RAM (core: IsBelowSwitch2RamMinimum). */
    private const val RAM_MINIMUM_GB = 16

    private fun askedKey(programId: String) = "switch2_phone_asked_$programId"

    fun isSwitch2(game: Game): Boolean =
        try {
            NativeLibrary.satronhoIsSwitch2(game.path)
        } catch (e: Exception) {
            false
        }

    /** The profile's resolution for this phone: 3/4 on the fastest GPUs, 1/2 on the rest. */
    private fun resolutionFor(family: Sugeridas.Family): Int = when (family) {
        Sugeridas.Family.AdrenoHigh, Sugeridas.Family.Xclipse -> Sugeridas.RES_3_4
        Sugeridas.Family.Adreno, Sugeridas.Family.Mali -> Sugeridas.RES_1_2
    }

    private fun resolutionName(activity: Activity, value: Int): String =
        activity.resources.getStringArray(R.array.rendererResolutionNames).getOrNull(value)
            ?: value.toString()

    /** Writes the profile into the game's own settings. False if they could not be saved. */
    private fun applyProfile(game: Game, resolution: Int): Boolean =
        try {
            SettingsFile.loadCustomConfig(game)
            IntSetting.RENDERER_RESOLUTION.setInt(resolution)
            IntSetting.RENDERER_SCALING_FILTER.setInt(Sugeridas.FILTER_FSR)
            BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
            NativeConfig.savePerGameConfig()
            NativeConfig.unloadPerGameConfig()
            true
        } catch (e: Exception) {
            try {
                NativeConfig.unloadPerGameConfig()
            } catch (ignored: Exception) {
            }
            false
        }

    /** Runs [next] right away for any other game, or after Troya's offer for a Switch 2 one. */
    fun beforeLaunch(activity: Activity, game: Game, next: () -> Unit) {
        val prefs = Satronho.prefs(activity)
        val key = askedKey(game.programId)
        if (game.programId.isEmpty() || prefs.getBoolean(key, false) || !isSwitch2(game)) {
            next()
            return
        }
        prefs.edit().putBoolean(key, true).apply()

        val device = Sugeridas.detect(activity)
        val resolution = resolutionFor(device.family)
        val message = StringBuilder(
            activity.getString(
                R.string.satronho_switch2_phone,
                game.title,
                resolutionName(activity, resolution)
            )
        )
        if (device.ramGb in 1 until RAM_MINIMUM_GB) {
            message.append("\n\n").append(
                activity.getString(R.string.satronho_switch2_phone_ram, device.ramGb, RAM_MINIMUM_GB)
            )
        }

        val speaker = Satronho.speaker("troya")
        val builder = MaterialAlertDialogBuilder(activity)
            .setTitle(activity.getString(R.string.satronho_switch2_phone_title, Satronho.guide(speaker).name))
            .setMessage(message.toString())
            .setPositiveButton(R.string.satronho_switch2_phone_apply) { _, _ ->
                if (applyProfile(game, resolution)) {
                    android.widget.Toast.makeText(
                        activity,
                        activity.getString(R.string.satronho_switch2_phone_applied, game.title),
                        android.widget.Toast.LENGTH_SHORT
                    ).show()
                }
                next()
            }
            .setNegativeButton(R.string.satronho_switch2_phone_keep) { _, _ -> next() }
            .setCancelable(false)
        if (Satronho.guidesEnabled) {
            Satronho.guideBitmap(activity, speaker)?.let { bitmap ->
                builder.setIcon(
                    RoundedBitmapDrawableFactory.create(activity.resources, bitmap)
                        .apply { isCircular = true }
                )
            }
        }
        builder.show()
    }
}
