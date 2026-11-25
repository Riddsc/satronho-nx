// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import android.content.Context
import android.graphics.Typeface
import android.util.TypedValue
import android.view.Gravity
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.model.Game

/**
 * The summary card when a game closes, as on the desktop: how long, average and low FPS, the
 * stutters and how many came from shaders. Troya tells it; Samy offers «Preparar juego» when the
 * shaders were the problem and the game isn't prepared yet.
 */
object ResumenSesion {
    fun duration(context: Context, seconds: Long): String {
        val minutes = seconds / 60
        return if (minutes >= 60) {
            context.getString(R.string.satronho_summary_hours, minutes / 60, minutes % 60)
        } else {
            context.getString(R.string.satronho_summary_minutes, maxOf(1L, minutes))
        }
    }

    /** One line for the game's card: «60 fps · 2 tirones». */
    fun cardLine(context: Context, resumen: SesionActual.Resumen): String =
        context.resources.getQuantityString(
            R.plurals.satronho_card_line, resumen.stutters,
            resumen.averageFps.toInt(), resumen.stutters
        )

    private fun row(context: Context, label: String, value: String, color: Int): LinearLayout =
        LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, Satronho.dp(context, 4), 0, Satronho.dp(context, 4))
            addView(
                TextView(context).apply {
                    text = label
                    setTextColor(Satronho.DIM)
                    setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
                },
                LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            )
            addView(TextView(context).apply {
                text = value
                setTextColor(color)
                typeface = Typeface.DEFAULT_BOLD
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 15f)
            })
        }

    /**
     * [prepared] says whether the game already went through «Preparar juego»; [onPrepare], when
     * given, is offered if the shaders caused most of the stutters.
     */
    fun show(
        activity: Activity,
        resumen: SesionActual.Resumen,
        prepared: Boolean,
        game: Game?,
        onPrepare: (() -> Unit)?
    ) {
        val context: Context = activity
        val pad = Satronho.dp(context, 20)
        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, Satronho.dp(context, 16), pad, 0)
        }

        val shaderProblem = resumen.shaderStutters > 0 &&
            resumen.shaderStutters * 2 >= resumen.stutters && !prepared && onPrepare != null &&
            !PrepareGame.nothingToPrepare(context, resumen.programId)
        val hot = resumen.maxTempC >= SesionActual.HOT_C
        val slow = !shaderProblem && game != null &&
            AjustePorJuego.shouldSuggest(context, resumen.programId)
        val speaker = Satronho.speaker(
            when {
                shaderProblem -> "samy"
                hot || slow -> "nono"
                else -> "troya"
            }
        )
        val guide = Satronho.guide(speaker)

        val header = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        val size = Satronho.dp(context, 48)
        header.addView(ImageView(context).apply {
            layoutParams = LinearLayout.LayoutParams(size, size)
            Satronho.guideBitmap(context, guide.key)?.let { bitmap ->
                setImageDrawable(
                    RoundedBitmapDrawableFactory.create(resources, bitmap).apply { isCircular = true }
                )
            }
        })
        header.addView(LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(Satronho.dp(context, 12), 0, 0, 0)
            addView(TextView(context).apply {
                text = context.getString(R.string.satronho_summary_title)
                setTextColor(guide.color)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
                typeface = Typeface.DEFAULT_BOLD
                isAllCaps = true
            })
            addView(TextView(context).apply {
                text = resumen.title
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 18f)
                typeface = Typeface.DEFAULT_BOLD
            })
        })
        root.addView(header)

        val list = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, Satronho.dp(context, 12), 0, 0)
        }
        val fpsColor = when {
            resumen.averageFps >= 28.0 -> Satronho.GREEN
            resumen.averageFps >= 20.0 -> Satronho.AMBER
            else -> Satronho.RED
        }
        list.addView(row(context, context.getString(R.string.satronho_summary_played),
            duration(context, resumen.seconds), Satronho.TEAL))
        list.addView(row(context, context.getString(R.string.satronho_summary_average),
            String.format("%.0f FPS", resumen.averageFps), fpsColor))
        list.addView(row(context, context.getString(R.string.satronho_summary_low),
            String.format("%.0f FPS", resumen.lowFps), Satronho.LILAC))
        val stutterText = if (resumen.shaderStutters > 0) {
            context.getString(R.string.satronho_summary_stutters_shaders, resumen.stutters,
                resumen.shaderStutters)
        } else {
            resumen.stutters.toString()
        }
        list.addView(row(context, context.getString(R.string.satronho_meter_stutters), stutterText,
            if (resumen.stutters == 0) Satronho.GREEN else Satronho.AMBER))
        if (resumen.worstMs > 0) {
            list.addView(row(context, context.getString(R.string.satronho_summary_worst),
                "${resumen.worstMs} ms", Satronho.PINK))
        }
        if (resumen.maxTempC > 0.0) {
            list.addView(row(context, context.getString(R.string.satronho_summary_temperature),
                String.format("%.0f °C", resumen.maxTempC),
                if (hot) Satronho.RED else Satronho.TEAL))
        }
        root.addView(list)

        val verdict = when {
            shaderProblem -> context.getString(R.string.satronho_summary_verdict_prepare)
            resumen.stutters == 0 -> context.getString(R.string.satronho_summary_verdict_smooth)
            resumen.shaderStutters * 2 >= resumen.stutters ->
                context.getString(R.string.satronho_summary_verdict_shaders)
            else -> context.getString(R.string.satronho_summary_verdict_other)
        }
        val notes = StringBuilder(verdict)
        if (hot) {
            notes.append("\n\n").append(
                context.getString(R.string.satronho_summary_hot, String.format("%.0f", resumen.maxTempC))
            )
        }
        if (slow) {
            notes.append("\n\n").append(context.getString(R.string.satronho_summary_slow))
        }
        root.addView(TextView(context).apply {
            text = notes
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            setPadding(0, Satronho.dp(context, 12), 0, Satronho.dp(context, 4))
        })

        val builder = MaterialAlertDialogBuilder(context)
            .setView(root)
            .setPositiveButton(R.string.satronho_close, null)
            .setNegativeButton(R.string.satronho_share_log) { _, _ -> Satronho.shareLatestLog(activity) }
        if (shaderProblem) {
            builder.setNeutralButton(R.string.satronho_prepare) { _, _ -> onPrepare?.invoke() }
        } else if (slow && game != null) {
            builder.setNeutralButton(R.string.satronho_nono_lower) { _, _ ->
                val changed = AjustePorJuego.lowerResolution(context, game)
                android.widget.Toast.makeText(
                    context,
                    changed ?: context.getString(R.string.satronho_nono_lowest),
                    android.widget.Toast.LENGTH_LONG
                ).show()
            }
        } else if (hot) {
            builder.setNeutralButton(R.string.satronho_summary_lighter) { _, _ -> Sugeridas.show(context) }
        } else {
            builder.setNeutralButton(R.string.satronho_summary_turn_off) { _, _ ->
                Satronho.summaryEnabled = false
            }
        }
        builder.show()
    }
}
