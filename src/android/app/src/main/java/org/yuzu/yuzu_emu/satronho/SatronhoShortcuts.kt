// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.content.Context
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.graphics.drawable.IconCompat
import org.yuzu.yuzu_emu.R

/**
 * Long-pressing the app icon shows «Satronho» (the Satronho screen) next to the last games played,
 * which the games list already adds as they are opened.
 */
object SatronhoShortcuts {
    private const val ID = "satronho_screen"

    fun publish(context: Context) {
        try {
            val icon = Satronho.guideBitmap(context, "mecharidds")
                ?.let { IconCompat.createWithAdaptiveBitmap(padded(it)) }
                ?: IconCompat.createWithResource(context, R.mipmap.ic_launcher)
            val shortcut = ShortcutInfoCompat.Builder(context, ID)
                .setShortLabel(context.getString(R.string.satronho_screen))
                .setLongLabel(context.getString(R.string.satronho_screen_long))
                .setIcon(icon)
                .setIntent(Satronho.openIntent(context))
                .setRank(0)
                .build()
            ShortcutManagerCompat.pushDynamicShortcut(context, shortcut)
        } catch (e: Exception) {
            // A launcher without shortcuts: nothing to add.
        }
    }

    /** Adaptive icons crop the outer third; keep the guide whole in the middle. */
    private fun padded(source: android.graphics.Bitmap): android.graphics.Bitmap {
        val size = 216
        val out = android.graphics.Bitmap.createBitmap(size, size, android.graphics.Bitmap.Config.ARGB_8888)
        val canvas = android.graphics.Canvas(out)
        canvas.drawColor(0xFF1A1626.toInt())
        val inner = 120
        val offset = (size - inner) / 2
        canvas.drawBitmap(
            source,
            null,
            android.graphics.Rect(offset, offset, offset + inner, offset + inner),
            android.graphics.Paint(android.graphics.Paint.FILTER_BITMAP_FLAG)
        )
        return out
    }
}
