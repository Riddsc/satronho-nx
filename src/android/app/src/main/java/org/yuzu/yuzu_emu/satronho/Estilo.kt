// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import android.content.Context
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatDelegate
import com.google.android.material.slider.Slider
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.utils.NativeConfig

/**
 * The Satronho screen's «Estilo» tab: the app's color theme (Violeta, Noche on pure black for the
 * OLED screen, Ámbar), the on-screen controls' look, and how visible the planet behind the games is.
 */
object Estilo {
    enum class Tema(val nameId: Int, val descriptionId: Int, val swatch: Int, val staticColor: Int, val black: Boolean) {
        Violeta(R.string.satronho_theme_violet, R.string.satronho_theme_violet_description, Satronho.VIOLET, 0, false),
        Noche(R.string.satronho_theme_night, R.string.satronho_theme_night_description, 0xFF000000.toInt(), 0, true),
        Ambar(R.string.satronho_theme_amber, R.string.satronho_theme_amber_description, Satronho.AMBER, 7, false)
    }

    fun current(): Tema? = Tema.entries.firstOrNull {
        IntSetting.THEME.getInt() == 0 &&
            IntSetting.STATIC_THEME_COLOR.getInt() == it.staticColor &&
            BooleanSetting.BLACK_BACKGROUNDS.getBoolean() == it.black
    }

    fun apply(activity: Activity, tema: Tema) {
        IntSetting.THEME.setInt(0)
        IntSetting.STATIC_THEME_COLOR.setInt(tema.staticColor)
        BooleanSetting.BLACK_BACKGROUNDS.setBoolean(tema.black)
        // Our themes are made for the dark mode.
        IntSetting.THEME_MODE.setInt(AppCompatDelegate.MODE_NIGHT_YES)
        NativeConfig.saveGlobalConfig()
        activity.recreate()
    }

    private fun title(context: Context, textId: Int): TextView = TextView(context).apply {
        text = context.getString(textId)
        setTextColor(Satronho.VIOLET)
        setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
        typeface = Typeface.DEFAULT_BOLD
        isAllCaps = true
        setPadding(0, Satronho.dp(context, 14), 0, Satronho.dp(context, 4))
    }

    /** A pickable row: a color dot, a name, a line, and a ring when it is the one in use. */
    private fun option(
        context: Context,
        swatch: Int,
        name: String,
        description: String,
        selected: Boolean,
        onPick: () -> Unit
    ): LinearLayout = LinearLayout(context).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        val pad = Satronho.dp(context, 10)
        setPadding(pad, pad, pad, pad)
        background = GradientDrawable().apply {
            cornerRadius = Satronho.dp(context, 14).toFloat()
            setColor(if (selected) 0x338B5CF6 else 0x00000000)
            setStroke(Satronho.dp(context, if (selected) 2 else 1), if (selected) Satronho.VIOLET else 0x33B9B2CC)
        }
        val dot = Satronho.dp(context, 28)
        addView(View(context).apply {
            background = GradientDrawable().apply {
                shape = GradientDrawable.OVAL
                setColor(swatch)
                setStroke(Satronho.dp(context, 2), Satronho.LILAC)
            }
        }, LinearLayout.LayoutParams(dot, dot))
        addView(LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(Satronho.dp(context, 12), 0, 0, 0)
            addView(TextView(context).apply {
                text = name
                typeface = Typeface.DEFAULT_BOLD
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 15f)
            })
            addView(TextView(context).apply {
                text = description
                setTextColor(Satronho.DIM)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            })
        }, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        if (selected) {
            addView(TextView(context).apply {
                text = "✓"
                setTextColor(Satronho.GREEN)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 18f)
            })
        }
        isClickable = true
        setOnClickListener { onPick() }
    }

    private fun spaced(view: View): View {
        view.layoutParams = LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        ).apply { bottomMargin = Satronho.dp(view.context, 8) }
        return view
    }

    fun buildView(activity: Activity, rebuild: () -> Unit): View {
        val context: Context = activity
        val pad = Satronho.dp(context, 20)
        val list = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, 0, pad, Satronho.dp(context, 12))
        }

        list.addView(title(context, R.string.satronho_style_theme))
        val now = current()
        Tema.entries.forEach { tema ->
            list.addView(spaced(option(
                context, tema.swatch, context.getString(tema.nameId),
                context.getString(tema.descriptionId), tema == now
            ) { if (tema != now) apply(activity, tema) }))
        }

        list.addView(title(context, R.string.satronho_style_controls))
        val controls = listOf(
            Triple(Satronho.CONTROLS_SATRONHO, R.string.satronho_controls_satronho, Satronho.VIOLET),
            Triple(Satronho.CONTROLS_CLASSIC, R.string.satronho_controls_classic, 0xFF9E9E9E.toInt()),
            Triple(Satronho.CONTROLS_SUBTLE, R.string.satronho_controls_subtle, 0x40DCD4FF)
        )
        val descriptions = mapOf(
            Satronho.CONTROLS_SATRONHO to R.string.satronho_controls_satronho_description,
            Satronho.CONTROLS_CLASSIC to R.string.satronho_controls_classic_description,
            Satronho.CONTROLS_SUBTLE to R.string.satronho_controls_subtle_description
        )
        controls.forEach { (value, nameId, swatch) ->
            list.addView(spaced(option(
                context, swatch, context.getString(nameId),
                context.getString(descriptions.getValue(value)), Satronho.controlStyle == value
            ) {
                Satronho.controlStyle = value
                rebuild()
            }))
        }

        list.addView(title(context, R.string.satronho_background_logo))
        list.addView(TextView(context).apply {
            text = context.getString(R.string.satronho_background_logo_slider)
            setTextColor(Satronho.DIM)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
        })
        list.addView(Slider(context).apply {
            valueFrom = 0f
            valueTo = 100f
            stepSize = 1f
            value = Satronho.logoOpacity.toFloat()
            setLabelFormatter { value ->
                if (value <= 0f) context.getString(R.string.satronho_off) else "${value.toInt()} %"
            }
            addOnChangeListener { _, value, fromUser ->
                if (fromUser) {
                    Satronho.logoOpacity = value.toInt()
                }
            }
        })
        return ScrollView(context).apply { addView(list) }
    }
}
