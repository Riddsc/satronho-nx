// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/satronho/captura.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QStandardPaths>

namespace Satronho::Captura {

QString NextPath() {
    QDir pictures(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    pictures.mkpath(QStringLiteral("Satronho NX"));
    return pictures.filePath(
        QStringLiteral("Satronho NX/Satronho_%1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))));
}

void Stamp(QImage& picture, const QString& game, double fps) {
    if (picture.isNull()) {
        return;
    }
    if (picture.format() != QImage::Format_ARGB32_Premultiplied &&
        picture.format() != QImage::Format_RGB32) {
        picture = picture.convertToFormat(QImage::Format_RGB32);
    }
    QPainter painter(&picture);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const double band = std::max(56.0, picture.height() * 0.11);
    const double top = picture.height() - band;

    QLinearGradient shade(0, top - band * 0.6, 0, picture.height());
    shade.setColorAt(0.0, QColor(0x0F, 0x0D, 0x16, 0));
    shade.setColorAt(1.0, QColor(0x0F, 0x0D, 0x16, 230));
    painter.fillRect(QRectF(0, top - band * 0.6, picture.width(), band * 1.6), shade);

    const double margin = band * 0.25;
    const double logo_size = band * 0.7;
    const QImage logo(QStringLiteral(":/satronho/logo_fondo.png"));
    if (!logo.isNull()) {
        painter.drawImage(QRectF(margin, top + (band - logo_size) / 2, logo_size, logo_size), logo);
    }
    const double text_left = margin * 2 + logo_size;
    QFont name_font;
    name_font.setBold(true);
    name_font.setPixelSize(static_cast<int>(band * 0.3));
    painter.setFont(name_font);
    painter.setPen(QColor(0xDC, 0xD4, 0xFF));
    painter.drawText(QPointF(text_left, top + band * 0.45), QStringLiteral("Satronho NX"));
    QFont detail_font;
    detail_font.setPixelSize(static_cast<int>(band * 0.24));
    painter.setFont(detail_font);
    painter.setPen(QColor(0xF2, 0xEE, 0xFF));
    const QString line =
        fps > 0.5 ? QStringLiteral("%1 · %2 FPS").arg(game).arg(fps, 0, 'f', 0) : game;
    painter.drawText(QPointF(text_left, top + band * 0.8), line);
}

} // namespace Satronho::Captura
