// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.content.Context
import android.content.SharedPreferences
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.view.View
import android.widget.TextView
import kotlin.math.roundToInt

/** Satronho NX for Android: shared helpers, colors and preferences of our own features. */
object Satronho {
    private const val PREFS = "satronho"

    // The same colors as the desktop's meter and guides.
    const val VIOLET = 0xFF8B5CF6.toInt()
    const val TEAL = 0xFF4FD1C5.toInt()
    const val GREEN = 0xFF74D14C.toInt()
    const val AMBER = 0xFFFFB23E.toInt()
    const val PINK = 0xFFFF8FB5.toInt()
    const val LILAC = 0xFF9B8CFF.toInt()
    const val RED = 0xFFFF5C7A.toInt()
    const val DIM = 0xFFB9B2CC.toInt()

    val GUIDE_NAMES = linkedMapOf(
        "mecharidds" to "MechaRidds",
        "samy" to "Samy",
        "troya" to "Troya",
        "nono" to "Ñoño",
        "juana" to "Juana"
    )

    fun prefs(context: Context): SharedPreferences =
        context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    fun dp(context: Context, value: Int): Int =
        (value * context.resources.displayMetrics.density).roundToInt()

    fun readAsset(context: Context, name: String): String? =
        try {
            context.assets.open(name).bufferedReader().use { it.readText() }
        } catch (e: Exception) {
            null
        }

    fun guideBitmap(context: Context, guide: String): Bitmap? =
        try {
            context.assets.open("guias/$guide.png").use { BitmapFactory.decodeStream(it) }
        } catch (e: Exception) {
            null
        }

    var guidesEnabled: Boolean
        get() = flag("guides_enabled", true)
        set(value) = setFlag("guides_enabled", value)

    var backgroundLogo: Boolean
        get() = flag("background_logo", true)
        set(value) = setFlag("background_logo", value)

    var meterEnabled: Boolean
        get() = flag("meter_enabled", true)
        set(value) = setFlag("meter_enabled", value)

    private var appContext: Context? = null

    /** Called once from the application: the flags above read from it. */
    fun init(context: Context) {
        appContext = context.applicationContext
    }

    private fun flag(key: String, default: Boolean): Boolean =
        appContext?.let { prefs(it).getBoolean(key, default) } ?: default

    private fun setFlag(key: String, value: Boolean) {
        appContext?.let { prefs(it).edit().putBoolean(key, value).apply() }
    }

    /** A small rounded label, as the desktop's Nuevo / Mejora / Arreglo tags. */
    fun styleTag(view: TextView, color: Int) {
        val context = view.context
        view.setTextColor(0xFF0F0D16.toInt())
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
        view.paint.isFakeBoldText = true
        view.setPadding(dp(context, 8), dp(context, 2), dp(context, 8), dp(context, 2))
        view.background = GradientDrawable().apply {
            cornerRadius = dp(context, 10).toFloat()
            setColor(color)
        }
    }

    fun setVisible(view: View, visible: Boolean) {
        view.visibility = if (visible) View.VISIBLE else View.GONE
    }
}
