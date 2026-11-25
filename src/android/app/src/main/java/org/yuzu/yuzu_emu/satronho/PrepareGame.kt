// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.progressindicator.LinearProgressIndicator
import java.text.DateFormat
import java.util.Date
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.model.Game

/**
 * «Preparar juego»: Satronho NX takes the game's shaders out of its files now, and its next start
 * builds all of them on the loading screen with this phone's driver, so they don't stutter while
 * playing. A phone builds shaders far slower than a PC, so this matters even more here.
 */
object PrepareGame {
    private fun preparedKey(game: Game) = "prepared_${game.programId}"

    fun preparedAt(activity: Activity, game: Game): Long =
        Satronho.prefs(activity).getLong(preparedKey(game), 0L)

    /** «Preparar juego» found nothing to take out of this game. */
    fun nothingToPrepare(context: Context, programId: String): Boolean =
        Satronho.prefs(context).getBoolean("prepare_nothing_$programId", false)

    /** Prepared with at least one shader program (older previews marked games with none too). */
    fun isPrepared(context: Context, programId: String): Boolean =
        Satronho.prefs(context).getLong("prepared_programs_$programId", 0L) > 0L

    /**
     * The first time a game that isn't prepared is opened, Samy offers to prepare it first.
     * Asked once per game; [launch] opens the game as usual.
     */
    fun suggestBeforeLaunch(activity: Activity, game: Game, launch: () -> Unit) {
        val prefs = Satronho.prefs(activity)
        val askedKey = "prepare_asked_${game.programId}"
        if (game.programId.isEmpty() || isPrepared(activity, game.programId) ||
            prefs.getBoolean(askedKey, false) || !Satronho.guidesEnabled
        ) {
            launch()
            return
        }
        prefs.edit().putBoolean(askedKey, true).apply()
        val speaker = Satronho.speaker("samy")
        MaterialAlertDialogBuilder(activity)
            .setIcon(Satronho.guideBitmap(activity, speaker)?.let { bitmap ->
                RoundedBitmapDrawableFactory.create(activity.resources, bitmap)
                    .apply { isCircular = true }
            })
            .setTitle(activity.getString(R.string.satronho_prepare_suggest_title, Satronho.guide(speaker).name))
            .setMessage(activity.getString(R.string.satronho_prepare_suggest, game.title))
            .setPositiveButton(R.string.satronho_prepare_now) { _, _ -> start(activity, game, launch) }
            .setNegativeButton(R.string.satronho_play_now) { _, _ -> launch() }
            .show()
    }

    fun menuDetails(activity: Activity, game: Game): String {
        val at = preparedAt(activity, game)
        return if (at == 0L || !isPrepared(activity, game.programId)) {
            ""
        } else {
            activity.getString(
                R.string.satronho_prepare_ready_since,
                DateFormat.getDateInstance(DateFormat.MEDIUM).format(Date(at))
            )
        }
    }

    fun start(activity: Activity, game: Game, onPlay: (() -> Unit)? = null) {
        val context = activity
        val pad = Satronho.dp(context, 20)
        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, Satronho.dp(context, 8), pad, 0)
        }
        val stage = TextView(context).apply {
            text = context.getString(R.string.satronho_prepare_reading)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
        }
        val bar = LinearProgressIndicator(context).apply {
            isIndeterminate = true
            max = 1000
        }
        val note = TextView(context).apply {
            text = context.getString(R.string.satronho_prepare_note)
            setTextColor(Satronho.DIM)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setPadding(0, Satronho.dp(context, 10), 0, 0)
        }
        root.addView(stage)
        root.addView(bar, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        ).apply { topMargin = Satronho.dp(context, 10) })
        root.addView(note)

        val dialog = MaterialAlertDialogBuilder(context)
            .setTitle(context.getString(R.string.satronho_prepare_title, game.title))
            .setView(root)
            .setCancelable(false)
            .setNegativeButton(R.string.satronho_stop) { _, _ -> NativeLibrary.satronhoPrepareCancel() }
            .show()

        val handler = Handler(Looper.getMainLooper())
        var finished = false
        val poll = object : Runnable {
            override fun run() {
                if (finished) {
                    return
                }
                val progress = NativeLibrary.satronhoPrepareProgress()
                val name = NativeLibrary.satronhoPrepareStage()
                if (progress[1] > 0) {
                    bar.isIndeterminate = false
                    bar.setProgressCompat((progress[0] * 1000 / progress[1]).toInt(), true)
                    stage.text = context.getString(
                        R.string.satronho_prepare_stage, stageText(activity, name),
                        progress[0], progress[1]
                    )
                } else if (name.isNotEmpty()) {
                    bar.isIndeterminate = true
                    stage.text = stageText(activity, name)
                }
                handler.postDelayed(this, 250)
            }
        }
        handler.post(poll)

        Thread {
            val programs = NativeLibrary.satronhoPrepareGame(game.path, game.programId)
            val error = if (programs < 0) NativeLibrary.satronhoPrepareError() else ""
            handler.post {
                finished = true
                if (dialog.isShowing) {
                    dialog.dismiss()
                }
                if (programs > 0) {
                    Satronho.prefs(activity).edit()
                        .putLong(preparedKey(game), System.currentTimeMillis())
                        .putLong("prepared_programs_${game.programId}", programs).apply()
                } else if (programs == 0L) {
                    // Nothing to take out ahead (Celeste builds every shader while playing): no ✓,
                    // and Samy doesn't offer it again.
                    Satronho.prefs(activity).edit()
                        .putBoolean("prepare_asked_${game.programId}", true)
                        .putBoolean("prepare_nothing_${game.programId}", true)
                        .remove(preparedKey(game))
                        .remove("prepared_programs_${game.programId}").apply()
                }
                val message = when {
                    programs > 0 -> context.getString(R.string.satronho_prepare_done, programs)
                    programs == 0L -> context.getString(R.string.satronho_prepare_none)
                    error == "stopped" -> context.getString(R.string.satronho_prepare_stopped)
                    programs == -2L -> context.getString(R.string.satronho_prepare_busy)
                    else -> context.getString(R.string.satronho_prepare_failed, error)
                }
                val done = MaterialAlertDialogBuilder(context)
                    .setTitle(R.string.satronho_prepare)
                    .setMessage(message)
                if (onPlay != null && programs >= 0) {
                    done.setPositiveButton(R.string.satronho_play_now) { _, _ -> onPlay() }
                        .setNegativeButton(R.string.satronho_close, null)
                } else {
                    done.setPositiveButton(android.R.string.ok, null)
                }
                done.show()
            }
        }.start()
    }

    private fun stageText(activity: Activity, stage: String): String = when (stage) {
        "Reading game files" -> activity.getString(R.string.satronho_prepare_reading)
        "Extracting shaders" -> activity.getString(R.string.satronho_prepare_extracting)
        "Converting" -> activity.getString(R.string.satronho_prepare_converting)
        "Writing" -> activity.getString(R.string.satronho_prepare_writing)
        "Reading asset packs (MiB)" -> activity.getString(R.string.satronho_prepare_packs)
        else -> stage
    }
}
