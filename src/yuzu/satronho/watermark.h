// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <vector>

#include <QObject>
#include <QPixmap>

#include "common/common_types.h"

class QAbstractItemView;
class QMenu;

namespace Satronho {

/// Satronho NX's logo behind the game list: our planet with the guides, centred on the list,
/// tree, grid or carousel. Always there; how visible it is goes from 5 % up to 100 %
/// (UISettings::values.satronho_watermark_opacity, Satronho menu → «Logo de fondo»).
class Watermark : public QObject {
    Q_OBJECT

public:
    static constexpr u32 MinOpacity = 5;
    static constexpr u32 MaxOpacity = 100;
    static constexpr u32 DefaultOpacity = 15;

    explicit Watermark(QObject* parent);

    /// Paints the logo behind `view`'s items.
    void Attach(QAbstractItemView* view);

    /// Fills `menu` with the opacity choices. `changed` runs after one is picked.
    static void FillOptionsMenu(QMenu* menu, const std::function<void()>& changed);

    /// Repaints every view after the opacity changed.
    void Refresh();

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    const QPixmap& Scaled(int side);

    QPixmap logo;
    QPixmap scaled;
    int scaled_side = 0;
    std::vector<QAbstractItemView*> views;
};

} // namespace Satronho
