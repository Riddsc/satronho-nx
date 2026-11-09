// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "common/scm_rev.h"
#include "qt_common/config/uisettings.h"
#include "yuzu/satronho/novedades.h"

namespace Satronho {
namespace {

QString Tr(const char* text) {
    return QCoreApplication::translate("Satronho", text);
}

std::vector<BuildNotes> LoadNotes() {
    std::vector<BuildNotes> notes;
    QFile file(QStringLiteral(":/satronho/novedades.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return notes;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    for (const QJsonValue& value : document.object().value(QStringLiteral("builds")).toArray()) {
        const QJsonObject build = value.toObject();
        BuildNotes entry{
            .tag = build.value(QStringLiteral("tag")).toString(),
            .date = build.value(QStringLiteral("fecha")).toString(),
            .title = build.value(QStringLiteral("titulo")).toString(),
            .changes = {},
        };
        for (const QJsonValue& change : build.value(QStringLiteral("cambios")).toArray()) {
            const QJsonObject object = change.toObject();
            entry.changes.push_back({object.value(QStringLiteral("tipo")).toString(),
                                     object.value(QStringLiteral("texto")).toString()});
        }
        notes.push_back(std::move(entry));
    }
    return notes;
}

struct Label {
    QString text;
    QString foreground;
    QString background;
};

Label LabelFor(const QString& type) {
    if (type == QStringLiteral("nuevo")) {
        return {Tr("Nuevo"), QStringLiteral("#4ade80"), QStringLiteral("#163a26")};
    }
    if (type == QStringLiteral("mejora")) {
        return {Tr("Mejora"), QStringLiteral("#b4a5ff"), QStringLiteral("#2b2550")};
    }
    return {Tr("Arreglo"), QStringLiteral("#ffa94d"), QStringLiteral("#40290f")};
}

} // Anonymous namespace

const std::vector<BuildNotes>& AllNotes() {
    static const std::vector<BuildNotes> notes = LoadNotes();
    return notes;
}

QString CurrentBuildTag() {
    return QString::fromUtf8(Common::g_build_release_tag);
}

const BuildNotes* NotesFor(const QString& tag) {
    for (const BuildNotes& notes : AllNotes()) {
        if (notes.tag == tag) {
            return &notes;
        }
    }
    return nullptr;
}

QString NotesHtml(const std::vector<BuildNotes>& notes) {
    QString html;
    for (const BuildNotes& build : notes) {
        QString number = build.tag;
        number.replace(QStringLiteral("build-"), Tr("build "));
        html += QStringLiteral(
                    "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" "
                    "style=\"margin-bottom:6px\"><tr>"
                    "<td><span style=\"color:#f5b942; font-weight:700; font-size:15px\">%1</span></td>"
                    "<td align=\"right\"><span style=\"color:#8d88a6\">%2</span></td></tr></table>"
                    "<p style=\"font-size:17px; font-weight:700; margin:0 0 10px 0\">%3</p>")
                    .arg(number.toHtmlEscaped(), build.date.toHtmlEscaped(),
                         build.title.toHtmlEscaped());
        html += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"7\">");
        for (const Change& change : build.changes) {
            const Label label = LabelFor(change.type);
            html += QStringLiteral(
                        "<tr><td width=\"96\" valign=\"top\">"
                        "<span style=\"background-color:%2; color:%1; font-weight:700\">"
                        "&nbsp;&nbsp;%3&nbsp;&nbsp;</span></td>"
                        "<td valign=\"top\">%4</td></tr>")
                        .arg(label.foreground, label.background, label.text,
                             change.text.toHtmlEscaped());
        }
        html += QStringLiteral("</table><hr style=\"border:0; height:1px\"><br>");
    }
    return html;
}

bool HasUnseenNotes() {
    const QString tag = CurrentBuildTag();
    return !tag.isEmpty() && !AllNotes().empty() &&
           QString::fromStdString(UISettings::values.satronho_seen_novedades.GetValue()) != tag;
}

void MarkNotesSeen() {
    UISettings::values.satronho_seen_novedades.SetValue(CurrentBuildTag().toStdString());
}

} // namespace Satronho
