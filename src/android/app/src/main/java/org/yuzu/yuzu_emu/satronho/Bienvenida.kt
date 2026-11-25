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
import android.widget.ScrollView
import android.widget.TextView
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.utils.NativeConfig

/**
 * MechaRidds' welcome, once, after the first setup: the three things that make Satronho NX ready
 * (keys, a games folder, the suggested settings), each marked done or with where to do it.
 */
object Bienvenida {
    private const val SHOWN_KEY = "bienvenida_vista"

    fun pending(context: Context): Boolean = !Satronho.prefs(context).getBoolean(SHOWN_KEY, false)

    private fun step(context: Context, number: Int, title: String, detail: String, done: Boolean): LinearLayout =
        LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.TOP
            setPadding(0, Satronho.dp(context, 8), 0, Satronho.dp(context, 8))
            addView(
                TextView(context).apply {
                    text = if (done) "✓" else number.toString()
                    gravity = Gravity.CENTER
                    Satronho.styleTag(this, if (done) Satronho.GREEN else Satronho.AMBER)
                    minWidth = Satronho.dp(context, 28)
                },
                LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply { marginEnd = Satronho.dp(context, 12) }
            )
            addView(
                LinearLayout(context).apply {
                    orientation = LinearLayout.VERTICAL
                    addView(TextView(context).apply {
                        text = title
                        typeface = Typeface.DEFAULT_BOLD
                        setTextSize(TypedValue.COMPLEX_UNIT_SP, 15f)
                    })
                    addView(TextView(context).apply {
                        text = detail
                        setTextColor(Satronho.DIM)
                        setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
                    })
                },
                LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            )
        }

    fun show(activity: Activity) {
        val context: Context = activity
        Satronho.prefs(context).edit().putBoolean(SHOWN_KEY, true).apply()

        val keys = try {
            NativeLibrary.areKeysPresent()
        } catch (e: Exception) {
            false
        }
        val folders = try {
            NativeConfig.getGameDirs().isNotEmpty()
        } catch (e: Exception) {
            false
        }

        val pad = Satronho.dp(context, 20)
        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, Satronho.dp(context, 18), pad, 0)
        }
        val header = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        val size = Satronho.dp(context, 60)
        header.addView(ImageView(context).apply {
            layoutParams = LinearLayout.LayoutParams(size, size)
            Satronho.guideBitmap(context, "mecharidds")?.let { bitmap ->
                setImageDrawable(
                    RoundedBitmapDrawableFactory.create(resources, bitmap).apply { isCircular = true }
                )
            }
        })
        header.addView(TextView(context).apply {
            text = context.getString(R.string.satronho_welcome_title)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 19f)
            typeface = Typeface.DEFAULT_BOLD
            setTextColor(Satronho.GREEN)
            setPadding(Satronho.dp(context, 12), 0, 0, 0)
        })
        root.addView(header)
        root.addView(TextView(context).apply {
            text = context.getString(R.string.satronho_welcome_text)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            setPadding(0, Satronho.dp(context, 12), 0, Satronho.dp(context, 4))
        })
        root.addView(step(context, 1, context.getString(R.string.satronho_welcome_keys),
            context.getString(if (keys) R.string.satronho_welcome_keys_done else R.string.satronho_welcome_keys_todo),
            keys))
        root.addView(step(context, 2, context.getString(R.string.satronho_welcome_folder),
            context.getString(if (folders) R.string.satronho_welcome_folder_done else R.string.satronho_welcome_folder_todo),
            folders))
        root.addView(step(context, 3, context.getString(R.string.satronho_suggested),
            context.getString(R.string.satronho_welcome_suggested), false))
        root.addView(TextView(context).apply {
            text = context.getString(R.string.satronho_welcome_footer)
            setTextColor(Satronho.DIM)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setPadding(0, Satronho.dp(context, 6), 0, 0)
        })

        MaterialAlertDialogBuilder(context)
            .setView(ScrollView(context).apply { addView(root) })
            .setPositiveButton(R.string.satronho_welcome_start, null)
            .setNeutralButton(R.string.satronho_suggested) { _, _ -> Sugeridas.show(context) }
            .show()
    }
}
