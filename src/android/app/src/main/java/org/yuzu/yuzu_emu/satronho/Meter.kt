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
import org.yuzu.yuzu_emu.R

/**
 * Satronho NX's in-game meter: FPS, frame time, stutters (and how many while shaders were being
 * built) and the shaders still building, in our colors, as on the desktop.
 */
class Meter {
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

    /** The counts come from [SesionActual], which watches the game whether or not this shows. */
    fun text(context: Context, fps: Double, frameTimeMs: Double, shadersBuilding: Int): CharSequence {
        val now = SystemClock.elapsedRealtime()
        val session = SesionActual
        val recent = session.lastStutterAt != 0L && now - session.lastStutterAt < 5000
        val recentShader = session.lastShaderStutterAt != 0L && now - session.lastShaderStutterAt < 5000

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
        out.span(session.stutters.toString(), if (recent) Satronho.RED else Satronho.LILAC, true)
        if (session.shaderStutters > 0) {
            out.span(" " + context.getString(R.string.satronho_meter_by_shaders, session.shaderStutters), Satronho.DIM)
        }
        // As on the desktop: «Armando shaders N · tirón por shaders».
        if (shadersBuilding > 0) {
            out.separator()
            out.span(context.getString(R.string.satronho_meter_building) + " ", Satronho.DIM)
            out.span(shadersBuilding.toString(), if (recentShader) Satronho.RED else Satronho.AMBER, true)
            if (recentShader) {
                out.span(" · " + context.getString(R.string.satronho_meter_building_stutter), Satronho.RED)
            }
        }
        // The battery's temperature: a hot phone slows itself down (Tensor especially).
        if (session.tempC > 0.0) {
            out.separator()
            val color = when {
                session.tempC >= SesionActual.HOT_C -> Satronho.RED
                session.tempC >= SesionActual.HOT_C - 4.0 -> Satronho.AMBER
                else -> Satronho.TEAL
            }
            out.span(String.format("%.0f °C", session.tempC), color, true)
        }
        return out
    }
}
