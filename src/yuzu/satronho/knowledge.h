// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QString>
#include <QStringList>

namespace Satronho {

/// One topic of the guides' knowledge base (dist/satronho/conocimiento.json).
struct Topic {
    QString id;
    QString guide;     ///< Key of the guide who answers it
    QString area;      ///< Section name ("Actualizaciones", "Gráficos", ...)
    QString question;  ///< The question as shown to the user
    QString answer;
    QStringList phrases;  ///< Normalized example phrasings
    QStringList keys;     ///< Normalized keywords
    QStringList related;  ///< Ids of related topics
};

struct Answer {
    const Topic* topic = nullptr;        ///< Best match, or nullptr when unsure
    std::vector<const Topic*> others;    ///< Close matches to offer
};

/// Every topic, in the file's order.
const std::vector<Topic>& Topics();

/// The topic with this id, or nullptr.
const Topic* FindTopic(const QString& id);

/// Lowercase, without accents or punctuation, Spanish plurals reduced.
QString Normalize(const QString& text);

/// The topic that best answers `question`. Without a confident match, `topic` is null and
/// `others` holds the closest topics.
Answer Ask(const QString& question);

} // namespace Satronho
