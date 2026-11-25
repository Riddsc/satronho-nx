// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.content.Context
import android.graphics.Typeface
import android.os.SystemClock
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.style.ForegroundColorSpan
import android.text.style.StyleSpan
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R

/**
 * Satronho NX's in-game meter: FPS, frame time, stutters (and how many while shaders were being
 * built) and the shaders still building, in our colors, as on the desktop.
 */
class Meter {
    private var stutters = 0
    private var shaderStutters = 0
    private var lastStutterAt = 0L
    private var lastShaderStutterAt = 0L

    private fun SpannableStringBuilder.span(text: String, color: Int, bold: Boolean = false) {
        val start = length
        append(text)
        setSpan(ForegroundColorSpan(color), start, length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        if (bold) {
            setSpan(StyleSpan(Typeface.BOLD), start, length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
    }

    private fun SpannableStringBuilder.separator() {
        if (isNotEmpty()) {
            span("  ·  ", Satronho.DIM)
        }
    }

    fun text(context: Context, fps: Double, frameTimeMs: Double, shadersBuilding: Int): CharSequence {
        val now = SystemClock.elapsedRealtime()
        val taken = NativeLibrary.satronhoTakeStutters()
        if (taken.isNotEmpty() && taken[0] > 0) {
            stutters += taken[0]
            lastStutterAt = now
            if (shadersBuilding > 0) {
                shaderStutters += taken[0]
                lastShaderStutterAt = now
            }
        }
        val recent = now - lastStutterAt < 5000
        val recentShader = now - lastShaderStutterAt < 5000

        val out = SpannableStringBuilder()
        val fpsColor = when {
            fps >= 28.0 -> Satronho.GREEN
            fps >= 20.0 -> Satronho.AMBER
            else -> Satronho.RED
        }
        out.span("FPS ", Satronho.DIM)
        out.span(String.format("%.0f", fps), fpsColor, true)
        out.separator()
        out.span(String.format("%.1f", frameTimeMs), Satronho.TEAL, true)
        out.span(" ms", Satronho.DIM)
        out.separator()
        out.span(context.getString(R.string.satronho_meter_stutters) + " ", Satronho.DIM)
        out.span(stutters.toString(), if (recent) Satronho.RED else Satronho.LILAC, true)
        if (shaderStutters > 0) {
            out.span(" " + context.getString(R.string.satronho_meter_by_shaders, shaderStutters), Satronho.DIM)
        }
        if (shadersBuilding > 0) {
            out.separator()
            if (recentShader) {
                out.span(context.getString(R.string.satronho_meter_building_stutter) + " ", Satronho.DIM)
                out.span(shadersBuilding.toString(), Satronho.RED, true)
            } else {
                out.span("Shaders ", Satronho.DIM)
                out.span(shadersBuilding.toString(), Satronho.AMBER, true)
            }
        }
        return out
    }
}
