// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <utility>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QSet>

#include "yuzu/satronho/knowledge.h"

namespace Satronho {
namespace {

// Words that carry no topic: articles, pronouns, question words, common verbs.
const QSet<QString>& StopWords() {
    static const QSet<QString> words{
        QStringLiteral("a"),      QStringLiteral("al"),     QStringLiteral("como"),
        QStringLiteral("con"),    QStringLiteral("cual"),   QStringLiteral("cuando"),
        QStringLiteral("de"),     QStringLiteral("del"),    QStringLiteral("donde"),
        QStringLiteral("el"),     QStringLiteral("en"),     QStringLiteral("es"),
        QStringLiteral("esta"),   QStringLiteral("este"),   QStringLiteral("hay"),
        QStringLiteral("la"),     QStringLiteral("las"),    QStringLiteral("lo"),
        QStringLiteral("los"),    QStringLiteral("me"),     QStringLiteral("mi"),
        QStringLiteral("o"),      QStringLiteral("para"),   QStringLiteral("pero"),
        QStringLiteral("por"),    QStringLiteral("puedo"),  QStringLiteral("que"),
        QStringLiteral("se"),     QStringLiteral("si"),     QStringLiteral("sirve"),
        QStringLiteral("su"),     QStringLiteral("un"),     QStringLiteral("una"),
        QStringLiteral("uno"),    QStringLiteral("y"),      QStringLiteral("yo"),
        QStringLiteral("tengo"),  QStringLiteral("hace"),   QStringLiteral("hacer"),
        QStringLiteral("hago"),   QStringLiteral("tu"),     QStringLiteral("le"),
        QStringLiteral("sobre"),  QStringLiteral("algo"),   QStringLiteral("eso"),
        QStringLiteral("esto"),   QStringLiteral("ese"),    QStringLiteral("muy"),
        QStringLiteral("mas"),    QStringLiteral("ya"),     QStringLiteral("no"),
        QStringLiteral("quiero"), QStringLiteral("necesito"), QStringLiteral("saber"),
        QStringLiteral("explica"), QStringLiteral("explicame"), QStringLiteral("dime"),
        QStringLiteral("ayuda"),  QStringLiteral("hola"),   QStringLiteral("porfa"),
        QStringLiteral("favor"),
    };
    return words;
}

// Reduces a word so "configuraciones" and "configuracion", "guias" and "guia" meet.
QString Stem(QString word) {
    if (word.size() > 5 && word.endsWith(QStringLiteral("iones"))) {
        word.chop(2);
    } else if (word.size() > 4 && word.endsWith(QStringLiteral("es")) &&
               !word.endsWith(QStringLiteral("tes"))) {
        word.chop(2);
    } else if (word.size() > 3 && word.endsWith(QLatin1Char('s'))) {
        word.chop(1);
    }
    return word;
}

QStringList Tokens(const QString& text) {
    QStringList tokens;
    for (const QString& word : Normalize(text).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (!StopWords().contains(word)) {
            tokens.push_back(Stem(word));
        }
    }
    return tokens;
}

QStringList NormalizedList(const QJsonValue& value, bool stem) {
    QStringList list;
    for (const QJsonValue& item : value.toArray()) {
        QString text = Normalize(item.toString());
        if (stem) {
            QStringList words;
            for (const QString& word : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
                words.push_back(Stem(word));
            }
            text = words.join(QLatin1Char(' '));
        }
        if (!text.isEmpty()) {
            list.push_back(text);
        }
    }
    return list;
}

std::vector<Topic> LoadTopics() {
    std::vector<Topic> topics;
    QFile file(QStringLiteral(":/satronho/conocimiento.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return topics;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    for (const QJsonValue& value :
         document.object().value(QStringLiteral("entradas")).toArray()) {
        const QJsonObject entry = value.toObject();
        Topic topic{
            .id = entry.value(QStringLiteral("id")).toString(),
            .guide = entry.value(QStringLiteral("guia")).toString(),
            .area = entry.value(QStringLiteral("tema")).toString(),
            .question = entry.value(QStringLiteral("pregunta")).toString(),
            .answer = entry.value(QStringLiteral("respuesta")).toString(),
            .phrases = NormalizedList(entry.value(QStringLiteral("frases")), true),
            .keys = NormalizedList(entry.value(QStringLiteral("claves")), true),
            .related = {},
        };
        for (const QJsonValue& id : entry.value(QStringLiteral("ver")).toArray()) {
            topic.related.push_back(id.toString());
        }
        // The question itself is also a phrasing.
        QStringList question_words;
        for (const QString& word : Normalize(topic.question).split(QLatin1Char(' '),
                                                                   Qt::SkipEmptyParts)) {
            question_words.push_back(Stem(word));
        }
        topic.phrases.push_back(question_words.join(QLatin1Char(' ')));
        topics.push_back(std::move(topic));
    }
    return topics;
}

// In how many topics each keyword appears: a keyword only one topic has weighs more.
const QHash<QString, int>& KeywordTopics();

// How well `tokens` (the user's words) fit a topic.
double Score(const Topic& topic, const QStringList& tokens) {
    if (tokens.isEmpty()) {
        return 0.0;
    }
    const QString joined = tokens.join(QLatin1Char(' '));
    double score = 0.0;
    // A whole example phrasing inside the question weighs most.
    for (const QString& phrase : topic.phrases) {
        QStringList content;
        for (const QString& word : phrase.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            if (!StopWords().contains(word)) {
                content.push_back(word);
            }
        }
        if (content.isEmpty()) {
            continue;
        }
        int found = 0;
        for (const QString& word : content) {
            if (tokens.contains(word)) {
                ++found;
            }
        }
        if (found == content.size()) {
            // A one-word phrasing counts like a weak keyword; longer ones weigh more and more.
            score = (std::max)(score, content.size() == 1 ? 1.0 : 2.0 + 1.5 * content.size());
        } else if (content.size() >= 2 && found >= content.size() - 1 && found >= 2) {
            score = (std::max)(score, 1.5 + found);
        }
        if (content.size() >= 2 && joined.contains(content.join(QLatin1Char(' ')))) {
            score += 1.0;
        }
    }
    // Then each keyword the user wrote, or else a word that starts the same (5+ letters).
    for (const QString& token : tokens) {
        if (topic.keys.contains(token)) {
            score += KeywordTopics().value(token) == 1 ? 3.0 : 2.0;
            continue;
        }
        for (const QString& key : topic.keys) {
            if (token.size() >= 5 && key.size() >= 5 &&
                (token.startsWith(key.left(5)) || key.startsWith(token.left(5)))) {
                score += 1.0;
                break;
            }
        }
    }
    return score;
}

const QHash<QString, int>& KeywordTopics() {
    static const QHash<QString, int> counts = [] {
        QHash<QString, int> result;
        for (const Topic& topic : Topics()) {
            QSet<QString> unique(topic.keys.begin(), topic.keys.end());
            for (const QString& key : unique) {
                ++result[key];
            }
        }
        return result;
    }();
    return counts;
}

} // Anonymous namespace

QString Normalize(const QString& text) {
    const QString decomposed = text.toLower().normalized(QString::NormalizationForm_D);
    QString plain;
    plain.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c.category() != QChar::Mark_NonSpacing) {
            plain.push_back(c);  // Accent marks dropped: "ñ" becomes "n", "á" becomes "a"
        }
    }
    QString out;
    out.reserve(plain.size());
    for (qsizetype i = 0; i < plain.size(); ++i) {
        const QChar c = plain[i];
        if (c.isLetterOrNumber()) {
            out.push_back(c);
        } else if (c == QLatin1Char('.') && i > 0 && i + 1 < plain.size() &&
                   plain[i - 1].isDigit() && plain[i + 1].isDigit()) {
            out.push_back(c);  // "1.5x"
        } else {
            out.push_back(QLatin1Char(' '));
        }
    }
    return out.simplified();
}

const std::vector<Topic>& Topics() {
    static const std::vector<Topic> topics = LoadTopics();
    return topics;
}

const Topic* FindTopic(const QString& id) {
    for (const Topic& topic : Topics()) {
        if (topic.id == id) {
            return &topic;
        }
    }
    return nullptr;
}

Answer Ask(const QString& question) {
    const QStringList tokens = Tokens(question);
    std::vector<std::pair<double, const Topic*>> scored;
    for (const Topic& topic : Topics()) {
        const double score = Score(topic, tokens);
        if (score > 0.0) {
            scored.emplace_back(score, &topic);
        }
    }
    std::ranges::stable_sort(scored, [](const auto& a, const auto& b) { return a.first > b.first; });

    Answer answer;
    // Confident: clearly ahead of the next topic, or the only topic a keyword points to.
    if (!scored.empty()) {
        const double top = scored.front().first;
        const double second = scored.size() > 1 ? scored[1].first : 0.0;
        if ((top >= 4.0 && (top - second >= 1.0 || top >= 7.0)) ||
            (top >= 3.0 && top - second >= 1.0) || (top >= 2.0 && second == 0.0)) {
            answer.topic = scored.front().second;
        }
    }
    for (size_t i = answer.topic ? 1 : 0; i < scored.size() && answer.others.size() < 3; ++i) {
        if (scored[i].first >= 2.0) {
            answer.others.push_back(scored[i].second);
        }
    }
    return answer;
}

} // namespace Satronho
