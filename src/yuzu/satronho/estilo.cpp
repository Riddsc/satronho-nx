// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/satronho/estilo.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace Satronho::Estilo {

namespace {

struct Colors {
    QColor window;
    QColor base;
    QColor alternate;
    QColor button;
    QColor text;
    QColor dim;
    QColor accent;
    QColor accent_text;
};

Colors ColorsFor(const QString& theme) {
    if (theme == QStringLiteral("satronho_noche")) {
        return {QColor(0x00, 0x00, 0x00), QColor(0x0A, 0x09, 0x10), QColor(0x12, 0x10, 0x1C),
                QColor(0x16, 0x13, 0x22), QColor(0xF2, 0xEE, 0xFF), QColor(0x8A, 0x84, 0x9E),
                QColor(0x8B, 0x5C, 0xF6), QColor(0xFF, 0xFF, 0xFF)};
    }
    if (theme == QStringLiteral("satronho_ambar")) {
        return {QColor(0x1C, 0x16, 0x10), QColor(0x14, 0x10, 0x0B), QColor(0x22, 0x1B, 0x13),
                QColor(0x2A, 0x21, 0x17), QColor(0xFF, 0xF4, 0xE6), QColor(0xB0, 0x9C, 0x84),
                QColor(0xFF, 0xB2, 0x3E), QColor(0x1C, 0x12, 0x04)};
    }
    return {QColor(0x15, 0x12, 0x25), QColor(0x0F, 0x0D, 0x1A), QColor(0x1B, 0x17, 0x2E),
            QColor(0x22, 0x1C, 0x3A), QColor(0xF2, 0xEE, 0xFF), QColor(0x9A, 0x93, 0xB5),
            QColor(0x8B, 0x5C, 0xF6), QColor(0xFF, 0xFF, 0xFF)};
}

/// The platform's own style and palette, kept from before we first changed them.
struct Original {
    bool saved{};
    QString style;
    QPalette palette;
};

Original& Saved() {
    static Original original;
    return original;
}

} // Anonymous namespace

bool IsOurs(const QString& theme) {
    return theme.startsWith(QStringLiteral("satronho_"));
}

bool Apply(const QString& theme) {
    Original& original = Saved();
    if (!original.saved) {
        original.saved = true;
        original.style = QApplication::style()->name();
        original.palette = QApplication::palette();
    }
    if (!IsOurs(theme)) {
        if (QApplication::style()->name() != original.style) {
            QApplication::setStyle(QStyleFactory::create(original.style));
            QApplication::setPalette(original.palette);
        }
        return false;
    }

    const Colors c = ColorsFor(theme);
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette palette;
    palette.setColor(QPalette::Window, c.window);
    palette.setColor(QPalette::WindowText, c.text);
    palette.setColor(QPalette::Base, c.base);
    palette.setColor(QPalette::AlternateBase, c.alternate);
    palette.setColor(QPalette::ToolTipBase, c.button);
    palette.setColor(QPalette::ToolTipText, c.text);
    palette.setColor(QPalette::PlaceholderText, c.dim);
    palette.setColor(QPalette::Text, c.text);
    palette.setColor(QPalette::Button, c.button);
    palette.setColor(QPalette::ButtonText, c.text);
    palette.setColor(QPalette::BrightText, c.accent);
    palette.setColor(QPalette::Highlight, c.accent);
    palette.setColor(QPalette::HighlightedText, c.accent_text);
    palette.setColor(QPalette::Link, c.accent.lighter(130));
    palette.setColor(QPalette::LinkVisited, c.accent);
    palette.setColor(QPalette::Light, c.button.lighter(150));
    palette.setColor(QPalette::Midlight, c.button.lighter(125));
    palette.setColor(QPalette::Mid, c.button.darker(120));
    palette.setColor(QPalette::Dark, c.button.darker(160));
    palette.setColor(QPalette::Shadow, QColor(0, 0, 0));
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, c.dim);
    }
    palette.setColor(QPalette::Disabled, QPalette::Highlight, c.button.lighter(130));
    QApplication::setPalette(palette);
    return true;
}

} // namespace Satronho::Estilo
