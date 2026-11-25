// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.widget.RemoteViews
import java.io.File
import java.io.FileOutputStream
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.ui.main.MainActivity

/**
 * «Continuar»: a home screen widget with the last game played, its art, how its last session went
 * and a button straight into it.
 */
class ContinuarWidget : AppWidgetProvider() {
    override fun onUpdate(context: Context, manager: AppWidgetManager, ids: IntArray) {
        ids.forEach { manager.updateAppWidget(it, views(context)) }
    }

    companion object {
        private const val ICON_FILE = "widget_continuar.png"

        /** Called when a game is opened from the list: it becomes the widget's game. */
        fun remember(context: Context, game: Game, icon: Bitmap?) {
            Satronho.prefs(context).edit()
                .putString("widget_title", game.title)
                .putString("widget_program", game.programId)
                .putString("widget_intent", game.launchIntent.toUri(Intent.URI_INTENT_SCHEME))
                .apply()
            if (icon != null) {
                try {
                    FileOutputStream(File(context.filesDir, ICON_FILE)).use {
                        Bitmap.createScaledBitmap(icon, 256, 256, true)
                            .compress(Bitmap.CompressFormat.PNG, 100, it)
                    }
                } catch (e: Exception) {
                    // The widget keeps the previous art.
                }
            }
            refresh(context)
        }

        fun refresh(context: Context) {
            try {
                val manager = AppWidgetManager.getInstance(context)
                val ids = manager.getAppWidgetIds(ComponentName(context, ContinuarWidget::class.java))
                ids.forEach { manager.updateAppWidget(it, views(context)) }
            } catch (e: Exception) {
                // No widget placed.
            }
        }

        private fun views(context: Context): RemoteViews {
            val views = RemoteViews(context.packageName, R.layout.widget_continuar)
            val prefs = Satronho.prefs(context)
            val title = prefs.getString("widget_title", null)
            val programId = prefs.getString("widget_program", null)
            val launch = prefs.getString("widget_intent", null)?.let {
                try {
                    Intent.parseUri(it, Intent.URI_INTENT_SCHEME)
                } catch (e: Exception) {
                    null
                }
            }

            if (title == null || launch == null) {
                views.setTextViewText(R.id.widget_title, context.getString(R.string.app_name))
                views.setTextViewText(R.id.widget_line, context.getString(R.string.satronho_widget_empty))
                views.setImageViewResource(R.id.widget_icon, R.drawable.ic_yuzu)
                views.setTextViewText(R.id.widget_button, context.getString(R.string.satronho_widget_open))
                val open = PendingIntent.getActivity(
                    context, 0, Intent(context, MainActivity::class.java),
                    PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
                )
                views.setOnClickPendingIntent(R.id.widget_root, open)
                views.setOnClickPendingIntent(R.id.widget_button, open)
                return views
            }

            views.setTextViewText(R.id.widget_title, title)
            val last = programId?.let { SesionActual.last(context, it) }
            views.setTextViewText(
                R.id.widget_line,
                last?.let { ResumenSesion.cardLine(context, it) }
                    ?: context.getString(R.string.satronho_widget_ready)
            )
            val icon = File(context.filesDir, ICON_FILE)
            if (icon.exists()) {
                BitmapFactory.decodeFile(icon.path)?.let { views.setImageViewBitmap(R.id.widget_icon, it) }
            } else {
                views.setImageViewResource(R.id.widget_icon, R.drawable.ic_yuzu)
            }
            views.setTextViewText(R.id.widget_button, context.getString(R.string.satronho_widget_continue))
            launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            val play = PendingIntent.getActivity(
                context, 1, launch,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            views.setOnClickPendingIntent(R.id.widget_button, play)
            views.setOnClickPendingIntent(R.id.widget_icon, play)
            val open = PendingIntent.getActivity(
                context, 0, Intent(context, MainActivity::class.java),
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            views.setOnClickPendingIntent(R.id.widget_root, open)
            return views
        }
    }
}
