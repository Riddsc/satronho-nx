// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/satronho/watermark.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QActionGroup>
#include <QEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>

#include "qt_common/config/uisettings.h"

namespace Satronho {

namespace {

u32 Opacity() {
    return std::clamp(UISettings::values.satronho_watermark_opacity.GetValue(),
                      Watermark::MinOpacity, Watermark::MaxOpacity);
}

} // namespace

Watermark::Watermark(QObject* parent)
    : QObject(parent), logo(QStringLiteral(":/satronho/logo_fondo.png")) {}

void Watermark::Attach(QAbstractItemView* view) {
    // The tree's zebra rows are opaque and would cut the logo into stripes.
    view->setAlternatingRowColors(false);
    // Scrolling copies the painted pixels along; the logo stays put, so repaint everything.
    const auto repaint = [view] { view->viewport()->update(); };
    connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, repaint);
    connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this, repaint);
    view->viewport()->installEventFilter(this);
    views.push_back(view);
}

void Watermark::Refresh() {
    for (QAbstractItemView* view : views) {
        view->viewport()->update();
    }
}

const QPixmap& Watermark::Scaled(int side) {
    if (side != scaled_side) {
        scaled = logo.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled_side = side;
    }
    return scaled;
}

bool Watermark::eventFilter(QObject* object, QEvent* event) {
    // The viewport's background is already filled when its paint event arrives; the logo goes on
    // it here, and the view then paints the games over it.
    if (event->type() == QEvent::Paint && !logo.isNull()) {
        auto* viewport = static_cast<QWidget*>(object);
        const QRect area = viewport->rect();
        const int side = std::min(std::min(area.width(), area.height()) * 3 / 5, 560);
        if (side >= 48) {
            const QPixmap& pixmap = Scaled(side);
            QPainter painter(viewport);
            painter.setOpacity(Opacity() / 100.0);
            painter.drawPixmap(area.center().x() - pixmap.width() / 2,
                               area.center().y() - pixmap.height() / 2, pixmap);
        }
    }
    return QObject::eventFilter(object, event);
}

void Watermark::FillOptionsMenu(QMenu* menu, const std::function<void()>& changed) {
    auto* group = new QActionGroup(menu);
    for (const u32 percent : {5u, 10u, 15u, 20u, 25u, 30u, 40u, 50u, 60u, 75u, 100u}) {
        QAction* action = menu->addAction(QStringLiteral("%1 %").arg(percent));
        action->setCheckable(true);
        action->setChecked(Opacity() == percent);
        group->addAction(action);
        QObject::connect(action, &QAction::triggered, menu, [percent, changed] {
            UISettings::values.satronho_watermark_opacity.SetValue(percent);
            changed();
        });
    }
}

} // namespace Satronho
