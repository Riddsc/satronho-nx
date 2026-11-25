// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.graphics.BitmapFactory
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory
import org.yuzu.yuzu_emu.R

/** Satronho NX's loading screen: our planet behind the game's art, and one guide's tip. */
object Carga {
    private fun tips(guide: String): Int = when (guide) {
        "samy" -> R.array.satronho_tips_samy
        "troya" -> R.array.satronho_tips_troya
        "nono" -> R.array.satronho_tips_nono
        "juana" -> R.array.satronho_tips_juana
        else -> R.array.satronho_tips_mecharidds
    }

    fun decorate(logo: ImageView, tipRow: View, guideView: ImageView, tipView: TextView) {
        val context = logo.context
        try {
            context.assets.open("logo_fondo.png").use { logo.setImageBitmap(BitmapFactory.decodeStream(it)) }
        } catch (e: Exception) {
            logo.visibility = View.GONE
        }
        if (!Satronho.guidesEnabled) {
            tipRow.visibility = View.GONE
            return
        }
        val guide = Satronho.pickPet()
        val list = context.resources.getStringArray(tips(guide))
        if (list.isEmpty()) {
            tipRow.visibility = View.GONE
            return
        }
        Satronho.guideBitmap(context, guide)?.let { bitmap ->
            guideView.setImageDrawable(
                RoundedBitmapDrawableFactory.create(context.resources, bitmap).apply { isCircular = true }
            )
        }
        val name = Satronho.guide(guide)
        tipView.text = "${name.name}: ${list.random()}"
        tipView.setTextColor(Satronho.DIM)
        tipRow.visibility = View.VISIBLE
    }
}
