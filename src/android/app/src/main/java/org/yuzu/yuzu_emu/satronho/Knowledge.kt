// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.content.Context
import java.text.Normalizer
import org.json.JSONArray
import org.json.JSONObject

/**
 * The guides' local knowledge for Android (conocimiento_android.json): the same matcher as the
 * desktop's (yuzu/satronho/knowledge.cpp), so a question finds its topic the same way.
 */
object Knowledge {
    data class Topic(
        val id: String,
        val guide: String,
        val area: String,
        val question: String,
        val answer: String,
        val phrases: List<String>,
        val keys: List<String>,
        val related: List<String>
    )

    data class Answer(val topic: Topic?, val others: List<Topic>)

    private val stopWords = setOf(
        "a", "al", "como", "con", "cual", "cuando", "de", "del", "donde", "el", "en", "es",
        "esta", "este", "hay", "la", "las", "lo", "los", "me", "mi", "o", "para", "pero", "por",
        "puedo", "que", "se", "si", "sirve", "su", "un", "una", "uno", "y", "yo", "tengo", "hace",
        "hacer", "hago", "tu", "le", "sobre", "algo", "eso", "esto", "ese", "muy", "mas", "ya",
        "no", "quiero", "necesito", "saber", "explica", "explicame", "dime", "ayuda", "hola",
        "porfa", "favor",
        // English
        "the", "an", "and", "or", "of", "to", "in", "on", "for", "with", "is", "are", "it",
        "my", "i", "do", "does", "how", "what", "where", "why", "can", "this", "that", "be",
        "please", "want", "need", "know", "tell", "help", "hi", "hello", "about", "you", "your"
    )

    private var topics: List<Topic>? = null
    private var keywordTopics: Map<String, Int> = emptyMap()

    private var topicsLanguage = ""

    fun topics(context: Context): List<Topic> {
        val language = if (Satronho.isSpanish(context)) "es" else "en"
        if (language != topicsLanguage) {
            topics = null
            topicsLanguage = language
        }
        topics?.let { return it }
        val loaded = load(context)
        topics = loaded
        val counts = HashMap<String, Int>()
        loaded.forEach { topic -> topic.keys.toSet().forEach { counts[it] = (counts[it] ?: 0) + 1 } }
        keywordTopics = counts
        return loaded
    }

    fun find(context: Context, id: String): Topic? = topics(context).firstOrNull { it.id == id }

    fun normalize(text: String): String {
        val plain = Normalizer.normalize(text.lowercase(), Normalizer.Form.NFD)
            .replace(Regex("\\p{Mn}+"), "")
        val out = StringBuilder(plain.length)
        for (i in plain.indices) {
            val c = plain[i]
            when {
                c.isLetterOrDigit() -> out.append(c)
                c == '.' && i > 0 && i + 1 < plain.length &&
                    plain[i - 1].isDigit() && plain[i + 1].isDigit() -> out.append(c)
                else -> out.append(' ')
            }
        }
        return out.toString().trim().replace(Regex("\\s+"), " ")
    }

    private fun stem(word: String): String = when {
        word.length > 5 && word.endsWith("iones") -> word.dropLast(2)
        word.length > 4 && word.endsWith("es") && !word.endsWith("tes") -> word.dropLast(2)
        word.length > 3 && word.endsWith("s") -> word.dropLast(1)
        else -> word
    }

    private fun words(text: String): List<String> =
        normalize(text).split(' ').filter { it.isNotEmpty() }

    private fun tokens(text: String): List<String> =
        words(text).filter { it !in stopWords }.map { stem(it) }

    private fun stemmedList(array: JSONArray?): List<String> {
        array ?: return emptyList()
        return (0 until array.length()).mapNotNull { i ->
            words(array.optString(i)).joinToString(" ") { stem(it) }.ifEmpty { null }
        }
    }

    private fun load(context: Context): List<Topic> {
        val text = Satronho.readLocalizedAsset(context, "conocimiento_android.json") ?: return emptyList()
        return try {
            val entries = JSONObject(text).getJSONArray("entradas")
            (0 until entries.length()).map { i ->
                val entry = entries.getJSONObject(i)
                val question = entry.optString("pregunta")
                val related = entry.optJSONArray("ver")
                Topic(
                    id = entry.optString("id"),
                    guide = entry.optString("guia"),
                    area = entry.optString("tema"),
                    question = question,
                    answer = entry.optString("respuesta"),
                    // The question itself is also a phrasing.
                    phrases = stemmedList(entry.optJSONArray("frases")) +
                        words(question).joinToString(" ") { stem(it) },
                    keys = stemmedList(entry.optJSONArray("claves")),
                    related = if (related == null) {
                        emptyList()
                    } else {
                        (0 until related.length()).map { related.optString(it) }
                    }
                )
            }
        } catch (e: Exception) {
            emptyList()
        }
    }

    private fun score(topic: Topic, tokens: List<String>): Double {
        if (tokens.isEmpty()) {
            return 0.0
        }
        val joined = tokens.joinToString(" ")
        var score = 0.0
        for (phrase in topic.phrases) {
            val content = phrase.split(' ').filter { it.isNotEmpty() && it !in stopWords }
            if (content.isEmpty()) {
                continue
            }
            val found = content.count { it in tokens }
            if (found == content.size) {
                score = maxOf(score, if (content.size == 1) 1.0 else 2.0 + 1.5 * content.size)
            } else if (content.size >= 2 && found >= content.size - 1 && found >= 2) {
                score = maxOf(score, 1.5 + found)
            }
            if (content.size >= 2 && joined.contains(content.joinToString(" "))) {
                score += 1.0
            }
        }
        for (token in tokens) {
            if (token in topic.keys) {
                score += if (keywordTopics[token] == 1) 3.0 else 2.0
                continue
            }
            for (key in topic.keys) {
                if (token.length >= 5 && key.length >= 5 &&
                    (token.startsWith(key.take(5)) || key.startsWith(token.take(5)))
                ) {
                    score += 1.0
                    break
                }
            }
        }
        return score
    }

    fun ask(context: Context, question: String): Answer {
        val all = topics(context)
        val words = tokens(question)
        val scored = all.map { score(it, words) to it }
            .filter { it.first > 0.0 }
            .sortedByDescending { it.first }
        var topic: Topic? = null
        if (scored.isNotEmpty()) {
            val top = scored[0].first
            val second = if (scored.size > 1) scored[1].first else 0.0
            if ((top >= 4.0 && (top - second >= 1.0 || top >= 7.0)) ||
                (top >= 3.0 && top - second >= 1.0) || (top >= 2.0 && second == 0.0)
            ) {
                topic = scored[0].second
            }
        }
        val others = scored.drop(if (topic != null) 1 else 0)
            .filter { it.first >= 2.0 }
            .take(3)
            .map { it.second }
        return Answer(topic, others)
    }
}
