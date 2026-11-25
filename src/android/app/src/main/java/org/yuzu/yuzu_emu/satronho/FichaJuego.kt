// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.core.content.FileProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.TaskState
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.GameIconUtils

/**
 * Each game's Satronho page (long press on its card): time played, its last sessions as a small
 * FPS chart, whether it is prepared, and Play, Prepare, a save backup to share, and its
 * properties.
 */
object FichaJuego {
    /** The last sessions' average FPS as bars, oldest on the left, colored like the meter. */
    private class FpsChart(context: Context, private val sessions: List<SesionActual.Resumen>) :
        View(context) {
        private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Satronho.DIM
            textSize = Satronho.dp(context, 10).toFloat()
            textAlign = Paint.Align.CENTER
        }

        override fun onDraw(canvas: Canvas) {
            super.onDraw(canvas)
            if (sessions.isEmpty()) {
                return
            }
            val label = Satronho.dp(context, 14).toFloat()
            val gap = Satronho.dp(context, 6).toFloat()
            val top = Satronho.dp(context, 4).toFloat()
            val ordered = sessions.reversed()
            val max = maxOf(60.0, ordered.maxOf { it.averageFps })
            val barWidth = (width - gap * (ordered.size + 1)) / ordered.size
            val chartHeight = height - label - top
            ordered.forEachIndexed { index, session ->
                val barHeight = (session.averageFps / max * chartHeight).toFloat()
                val left = gap + index * (barWidth + gap)
                paint.color = when {
                    session.averageFps >= 28.0 -> Satronho.GREEN
                    session.averageFps >= 20.0 -> Satronho.AMBER
                    else -> Satronho.RED
                }
                canvas.drawRoundRect(
                    RectF(left, top + chartHeight - barHeight, left + barWidth, top + chartHeight),
                    gap, gap, paint
                )
                canvas.drawText(
                    String.format("%.0f", session.averageFps),
                    left + barWidth / 2, height - Satronho.dp(context, 2).toFloat(), text
                )
            }
        }
    }

    private fun stat(context: Context, label: String, value: String, color: Int): LinearLayout =
        LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            addView(TextView(context).apply {
                text = value
                setTextColor(color)
                typeface = Typeface.DEFAULT_BOLD
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 17f)
                gravity = Gravity.CENTER
            })
            addView(TextView(context).apply {
                text = label
                setTextColor(Satronho.DIM)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
                gravity = Gravity.CENTER
            })
        }

    fun show(activity: Activity, game: Game, play: () -> Unit, openProperties: () -> Unit) {
        val context: Context = activity
        val pad = Satronho.dp(context, 20)
        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, Satronho.dp(context, 18), pad, Satronho.dp(context, 4))
        }

        // The game's art and name.
        val header = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        val iconSize = Satronho.dp(context, 76)
        header.addView(ImageView(context).apply {
            layoutParams = LinearLayout.LayoutParams(iconSize, iconSize)
            scaleType = ImageView.ScaleType.CENTER_CROP
            clipToOutline = true
            background = android.graphics.drawable.GradientDrawable().apply {
                cornerRadius = Satronho.dp(context, 14).toFloat()
                setColor(0xFF1A1626.toInt())
            }
            GameIconUtils.loadGameIcon(game, this)
        })
        header.addView(LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(Satronho.dp(context, 14), 0, 0, 0)
            addView(TextView(context).apply {
                text = game.title
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 19f)
                typeface = Typeface.DEFAULT_BOLD
            })
            if (game.developer.isNotEmpty()) {
                addView(TextView(context).apply {
                    text = game.developer
                    setTextColor(Satronho.DIM)
                    setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
                })
            }
            val prepared = PrepareGame.isPrepared(context, game.programId)
            addView(TextView(context).apply {
                text = context.getString(
                    when {
                        prepared -> R.string.satronho_card_prepared
                        PrepareGame.nothingToPrepare(context, game.programId) -> R.string.satronho_card_nothing
                        else -> R.string.satronho_card_not_prepared
                    }
                )
                Satronho.styleTag(this, if (prepared) Satronho.GREEN else Satronho.LILAC)
            }, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = Satronho.dp(context, 6) })
        })
        root.addView(header)

        // Numbers: time played, sessions, the last one's FPS and stutters.
        val history = SesionActual.history(context, game.programId)
        val last = history.firstOrNull()
        val stats = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, Satronho.dp(context, 16), 0, Satronho.dp(context, 8))
        }
        val weight = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
        stats.addView(stat(context, context.getString(R.string.satronho_card_time),
            ResumenSesion.duration(context, SesionActual.totalSeconds(context, game.programId)),
            Satronho.TEAL), weight)
        stats.addView(stat(context, context.getString(R.string.satronho_card_sessions),
            history.size.toString(), Satronho.LILAC), LinearLayout.LayoutParams(weight))
        stats.addView(stat(context, context.getString(R.string.satronho_card_last_fps),
            last?.let { String.format("%.0f", it.averageFps) } ?: "—", Satronho.GREEN),
            LinearLayout.LayoutParams(weight))
        stats.addView(stat(context, context.getString(R.string.satronho_meter_stutters),
            last?.stutters?.toString() ?: "—", Satronho.AMBER), LinearLayout.LayoutParams(weight))
        root.addView(stats)

        if (history.isNotEmpty()) {
            root.addView(TextView(context).apply {
                text = context.getString(R.string.satronho_card_chart)
                setTextColor(Satronho.DIM)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            })
            root.addView(FpsChart(context, history), LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, Satronho.dp(context, 96)
            ))
        } else {
            root.addView(TextView(context).apply {
                text = context.getString(R.string.satronho_card_no_sessions)
                setTextColor(Satronho.DIM)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            })
        }

        lateinit var dialog: androidx.appcompat.app.AlertDialog
        fun action(textId: Int, color: Int, run: () -> Unit): Button =
            Button(context).apply {
                text = context.getString(textId)
                setTextColor(color)
                isAllCaps = false
                setOnClickListener {
                    dialog.dismiss()
                    run()
                }
            }

        val actions = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, Satronho.dp(context, 12), 0, 0)
        }
        actions.addView(action(R.string.satronho_play_now, Satronho.GREEN, play))
        if (!PrepareGame.nothingToPrepare(context, game.programId)) {
            actions.addView(action(R.string.satronho_prepare, Satronho.AMBER) {
                PrepareGame.start(activity, game, play)
            })
        }
        if (!game.isHomebrew) {
            actions.addView(action(R.string.satronho_backup_saves, Satronho.TEAL) {
                backupSaves(activity, game)
            })
        }
        actions.addView(action(R.string.satronho_card_properties, Satronho.LILAC, openProperties))
        root.addView(actions)

        dialog = MaterialAlertDialogBuilder(context)
            .setView(ScrollView(context).apply { addView(root) })
            .setPositiveButton(R.string.satronho_close, null)
            .create()
        dialog.show()
    }

    /**
     * Packs the game's saves into a .zip and opens Android's share menu (Drive, WhatsApp, the PC).
     * Restoring is in the game's properties → Save data → Import.
     */
    fun backupSaves(activity: Activity, game: Game) {
        val saves = File(game.saveDir)
        if (!saves.exists()) {
            Toast.makeText(activity, R.string.satronho_backup_none, Toast.LENGTH_LONG).show()
            return
        }
        Toast.makeText(activity, R.string.satronho_backup_working, Toast.LENGTH_SHORT).show()
        val handler = Handler(Looper.getMainLooper())
        Thread {
            val folder = File(activity.cacheDir, "respaldos").apply { mkdirs() }
            folder.listFiles()?.forEach { it.delete() }
            val safeName = game.saveZipName.replace(Regex("[\\\\/:*?\"<>|]"), "_")
            val zip = File(folder, "$safeName.zip")
            val result = try {
                FileUtil.zipFromInternalStorage(
                    saves,
                    game.saveDir.replaceAfterLast("/", ""),
                    BufferedOutputStream(FileOutputStream(zip)),
                    compression = true
                )
            } catch (e: Exception) {
                TaskState.Failed
            }
            handler.post {
                if (result != TaskState.Completed || !zip.exists()) {
                    Toast.makeText(activity, R.string.satronho_backup_failed, Toast.LENGTH_LONG).show()
                    return@post
                }
                val uri = FileProvider.getUriForFile(
                    activity, "${activity.packageName}.provider", zip
                )
                val intent = Intent(Intent.ACTION_SEND)
                    .setType("application/zip")
                    .putExtra(Intent.EXTRA_STREAM, uri)
                    .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                activity.startActivity(
                    Intent.createChooser(intent, activity.getString(R.string.satronho_backup_saves))
                )
            }
        }.start()
    }
}
