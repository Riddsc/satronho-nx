// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import android.content.Context
import android.graphics.Typeface
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.json.JSONObject
import org.yuzu.yuzu_emu.R

/**
 * Satronho NX's «Novedades»: the Android list (novedades_android.json) first, then the desktop's
 * latest builds (novedades.json), the same file the desktop shows. Shown once per new list.
 */
object Novedades {
    data class Change(val type: String, val text: String)
    data class Build(val tag: String, val date: String, val title: String, val changes: List<Change>)

    private const val SEEN_KEY = "novedades_vistas"
    private const val DESKTOP_BUILDS = 4

    fun load(context: Context, file: String): List<Build> {
        val text = Satronho.readAsset(context, file) ?: return emptyList()
        return try {
            val builds = JSONObject(text).getJSONArray("builds")
            (0 until builds.length()).map { i ->
                val build = builds.getJSONObject(i)
                val changes = build.getJSONArray("cambios")
                Build(
                    tag = build.optString("tag"),
                    date = build.optString("fecha"),
                    title = build.optString("titulo"),
                    changes = (0 until changes.length()).map { j ->
                        val change = changes.getJSONObject(j)
                        Change(change.optString("tipo"), change.optString("texto"))
                    }
                )
            }
        } catch (e: Exception) {
            emptyList()
        }
    }

    private fun newestKey(context: Context): String {
        val android = load(context, "novedades_android.json").firstOrNull()?.tag ?: ""
        val desktop = load(context, "novedades.json").firstOrNull()?.tag ?: ""
        return "$android|$desktop"
    }

    fun hasUnseen(context: Context): Boolean =
        Satronho.prefs(context).getString(SEEN_KEY, null) != newestKey(context)

    fun markSeen(context: Context) {
        Satronho.prefs(context).edit().putString(SEEN_KEY, newestKey(context)).apply()
    }

    /** The news list: Android's first, then the desktop's latest (Spanish only). */
    fun buildView(context: Context): View {
        val list = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            val pad = Satronho.dp(context, 20)
            setPadding(pad, Satronho.dp(context, 8), pad, Satronho.dp(context, 8))
        }
        section(list, context.getString(R.string.satronho_novedades_android))
        load(context, localized(context, "novedades_android.json")).forEach { addBuild(list, it) }
        if (Satronho.isSpanish(context)) {
            section(list, context.getString(R.string.satronho_novedades_desktop))
            load(context, "novedades.json").take(DESKTOP_BUILDS).forEach { addBuild(list, it) }
        }
        return ScrollView(context).apply { addView(list) }
    }

    private fun localized(context: Context, file: String): String {
        if (Satronho.isSpanish(context)) {
            return file
        }
        val english = file.replace(".json", "_en.json")
        return if (Satronho.readAsset(context, english) != null) english else file
    }

    fun show(activity: Activity) {
        val context: Context = activity
        MaterialAlertDialogBuilder(context)
            .setTitle(R.string.satronho_novedades)
            .setView(buildView(context))
            .setPositiveButton(android.R.string.ok, null)
            .setOnDismissListener { markSeen(context) }
            .show()
    }

    private fun section(list: LinearLayout, title: String) {
        val context = list.context
        list.addView(TextView(context).apply {
            text = title
            setTextColor(Satronho.VIOLET)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            typeface = Typeface.DEFAULT_BOLD
            isAllCaps = true
            setPadding(0, Satronho.dp(context, 14), 0, Satronho.dp(context, 2))
        })
    }

    private fun addBuild(list: LinearLayout, build: Build) {
        val context = list.context
        list.addView(TextView(context).apply {
            text = if (build.title.isNotEmpty()) "${build.tag} · ${build.title}" else build.tag
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 17f)
            typeface = Typeface.DEFAULT_BOLD
            setPadding(0, Satronho.dp(context, 10), 0, 0)
        })
        if (build.date.isNotEmpty()) {
            list.addView(TextView(context).apply {
                text = build.date
                setTextColor(Satronho.DIM)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            })
        }
        build.changes.forEach { change ->
            val row = LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.TOP
                setPadding(0, Satronho.dp(context, 8), 0, 0)
            }
            val (label, color) = when (change.type) {
                "nuevo" -> context.getString(R.string.satronho_tag_new) to Satronho.GREEN
                "mejora" -> context.getString(R.string.satronho_tag_improvement) to Satronho.TEAL
                else -> context.getString(R.string.satronho_tag_fix) to Satronho.AMBER
            }
            row.addView(
                TextView(context).apply {
                    text = label
                    Satronho.styleTag(this, color)
                },
                LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply { marginEnd = Satronho.dp(context, 8) }
            )
            row.addView(
                TextView(context).apply {
                    text = change.text
                    setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
                },
                LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            )
            list.addView(row)
        }
    }
}
