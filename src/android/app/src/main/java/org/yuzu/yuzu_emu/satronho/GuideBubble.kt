// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import android.content.Context
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.text.InputType
import android.util.TypedValue
import android.view.Gravity
import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.EditorInfo
import android.widget.Button
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlin.random.Random
import org.yuzu.yuzu_emu.R

/**
 * The guides' floating bubble on the games list: MechaRidds and the pets answer questions about
 * Satronho NX's functions and settings from their local knowledge, and tell when there is news.
 */
object GuideBubble {
    private const val FIRST_SHOWN_KEY = "bubble_first_shown"

    /** The pet on the bubble this time: Samy the first time, then a weighted pick of the pets on. */
    private fun bubbleGuide(context: Context): String {
        val prefs = Satronho.prefs(context)
        if (!prefs.getBoolean(FIRST_SHOWN_KEY, false)) {
            prefs.edit().putBoolean(FIRST_SHOWN_KEY, true).apply()
            return Satronho.speaker("samy")
        }
        val pets = listOf("samy" to 33, "troya" to 32, "nono" to 18, "juana" to 17)
            .filter { Satronho.petEnabled(it.first) }
        if (pets.isEmpty()) {
            return "mecharidds"
        }
        var roll = Random.nextInt(pets.sumOf { it.second })
        for ((pet, weight) in pets) {
            if (roll < weight) {
                return pet
            }
            roll -= weight
        }
        return pets.last().first
    }

    private fun avatar(context: Context, guide: String, sizeDp: Int): ImageView {
        val size = Satronho.dp(context, sizeDp)
        return ImageView(context).apply {
            layoutParams = LinearLayout.LayoutParams(size, size)
            Satronho.guideBitmap(context, guide)?.let { bitmap ->
                setImageDrawable(
                    RoundedBitmapDrawableFactory.create(resources, bitmap).apply { isCircular = true }
                )
            }
            contentDescription = Satronho.GUIDE_NAMES[guide]
        }
    }

    fun setup(bubble: ImageView, activity: Activity) {
        if (!Satronho.guidesEnabled) {
            bubble.visibility = View.GONE
            return
        }
        val context = bubble.context
        val guide = bubbleGuide(context)
        Satronho.guideBitmap(context, guide)?.let { bitmap ->
            bubble.setImageDrawable(
                RoundedBitmapDrawableFactory.create(context.resources, bitmap)
                    .apply { isCircular = true }
            )
        }
        val ring = Satronho.dp(context, 3)
        bubble.setPadding(ring, ring, ring, ring)
        bubble.background = GradientDrawable().apply {
            shape = GradientDrawable.OVAL
            setColor(0xFF1A1626.toInt())
            setStroke(ring, if (Novedades.hasUnseen(context)) Satronho.AMBER else Satronho.VIOLET)
        }
        bubble.elevation = Satronho.dp(context, 6).toFloat()
        bubble.contentDescription = context.getString(R.string.satronho_guides)
        bubble.visibility = View.VISIBLE
        bubble.setOnClickListener { open(activity, guide) { setup(bubble, activity) } }
        bubble.setOnLongClickListener {
            SatronhoScreen.show(activity, onClose = { setup(bubble, activity) })
            true
        }
    }

    /** The conversation with the guides: their greeting, topics, answers and the question box. */
    fun buildChat(activity: Activity, guide: String): View {
        val context: Context = activity
        val pad = Satronho.dp(context, 20)
        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, Satronho.dp(context, 12), pad, Satronho.dp(context, 4))
        }

        val header = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        var headerAvatar = avatar(context, guide, 48)
        val headerName = TextView(context).apply {
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 18f)
            typeface = Typeface.DEFAULT_BOLD
            setPadding(Satronho.dp(context, 12), 0, 0, 0)
        }
        header.addView(headerAvatar)
        header.addView(headerName)
        root.addView(header)

        val speech = TextView(context).apply {
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 15f)
            setPadding(0, Satronho.dp(context, 12), 0, Satronho.dp(context, 8))
            setTextIsSelectable(true)
        }
        root.addView(speech)

        val links = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }
        root.addView(links)

        val input = EditText(context).apply {
            hint = context.getString(R.string.satronho_guides_hint)
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
            imeOptions = EditorInfo.IME_ACTION_SEND
            isSingleLine = true
        }
        root.addView(input)

        fun speak(speaker: String, text: String) {
            val index = header.indexOfChild(headerAvatar)
            header.removeView(headerAvatar)
            headerAvatar = avatar(context, speaker, 48)
            header.addView(headerAvatar, index)
            headerName.text = Satronho.GUIDE_NAMES[speaker] ?: speaker
            speech.text = text
        }

        fun showLinks(title: String, topics: List<Knowledge.Topic>, onPick: (Knowledge.Topic) -> Unit) {
            links.removeAllViews()
            if (topics.isEmpty()) {
                return
            }
            links.addView(TextView(context).apply {
                text = title
                setTextColor(Satronho.DIM)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            })
            topics.forEach { topic ->
                links.addView(TextView(context).apply {
                    text = "› ${topic.question}"
                    setTextColor(Satronho.TEAL)
                    setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
                    setPadding(0, Satronho.dp(context, 6), 0, Satronho.dp(context, 6))
                    setOnClickListener { onPick(topic) }
                })
            }
        }

        lateinit var answer: (Knowledge.Topic) -> Unit
        answer = { topic ->
            speak(Satronho.speaker(topic.guide), topic.answer)
            showLinks(
                context.getString(R.string.satronho_guides_related),
                topic.related.mapNotNull { Knowledge.find(context, it) },
                answer
            )
        }

        fun ask(question: String) {
            if (question.isBlank()) {
                return
            }
            val result = Knowledge.ask(context, question)
            if (result.topic != null) {
                answer(result.topic)
                if (result.others.isNotEmpty()) {
                    showLinks(context.getString(R.string.satronho_guides_maybe), result.others, answer)
                }
            } else if (result.others.isNotEmpty()) {
                speak("mecharidds", context.getString(R.string.satronho_guides_choose))
                showLinks(context.getString(R.string.satronho_guides_maybe), result.others, answer)
            } else {
                speak("mecharidds", context.getString(R.string.satronho_guides_unknown))
                showLinks(
                    context.getString(R.string.satronho_guides_topics),
                    Knowledge.topics(context).take(6),
                    answer
                )
            }
        }

        val greeting = StringBuilder(
            context.getString(
                R.string.satronho_guides_greeting,
                Satronho.GUIDE_NAMES[guide] ?: guide
            )
        )
        if (Novedades.hasUnseen(context)) {
            greeting.append("\n\n").append(context.getString(R.string.satronho_guides_news))
        }
        speak(guide, greeting.toString())
        showLinks(
            context.getString(R.string.satronho_guides_topics),
            Knowledge.topics(context).take(6),
            answer
        )

        input.setOnEditorActionListener { view, actionId, event ->
            val send = actionId == EditorInfo.IME_ACTION_SEND ||
                (event != null && event.keyCode == KeyEvent.KEYCODE_ENTER &&
                    event.action == KeyEvent.ACTION_DOWN)
            if (send) {
                ask(view.text.toString())
                view.text = ""
            }
            send
        }

        val buttons = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.END
            setPadding(0, Satronho.dp(context, 8), 0, 0)
        }
        val askButton = Button(context).apply {
            text = context.getString(R.string.satronho_guides_ask)
            setOnClickListener {
                ask(input.text.toString())
                input.setText("")
            }
        }
        buttons.addView(askButton)
        root.addView(buttons)
        return ScrollView(context).apply { addView(root) }
    }

    private fun open(activity: Activity, guide: String, onClose: () -> Unit) {
        MaterialAlertDialogBuilder(activity)
            .setView(buildChat(activity, guide))
            .setPositiveButton(R.string.satronho_close, null)
            .setNeutralButton(R.string.satronho_screen) { _, _ -> SatronhoScreen.show(activity, onClose) }
            .setNegativeButton(R.string.satronho_guides_turn_off) { _, _ ->
                Satronho.guidesEnabled = false
            }
            .setOnDismissListener { onClose() }
            .show()
    }
}
