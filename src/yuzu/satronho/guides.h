// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>

#include <QColor>
#include <QPixmap>
#include <QString>

class QWidget;

namespace Satronho {

/// MechaRidds and his four pets. MechaRidds is in every section; each pet owns one topic.
enum class GuideId { MechaRidds, Samy, Troya, Nono, Juana };

struct Guide {
    GuideId id;
    const char* key;   ///< Knowledge base and resource name ("mecharidds", "samy", ...)
    QString name;      ///< Display name
    QString role;      ///< What the guide takes care of
    QString about;     ///< One line about the guide
    QColor color;      ///< The ring colour of the guide in the icon
    int avatar_size;   ///< Avatar size in pixels, after the guide's real size (Ñoño XL ... Juana XS)
};

/// All five guides, MechaRidds first.
const std::array<Guide, 5>& Guides();

const Guide& GetGuide(GuideId id);

/// The guide whose knowledge base key is `key`; MechaRidds for an unknown key.
GuideId GuideFromKey(const QString& key);

/// Whether the guides are on at all.
bool GuidesEnabled();

/// Whether this guide shows up: the guides are on and, for a pet, the pet is on too.
bool GuideEnabled(GuideId id);

/// Turns a guide on or off; MechaRidds stands for all of them.
void SetGuideEnabled(GuideId id, bool enabled);

/// The guide that speaks for `id`: itself when on, MechaRidds otherwise.
GuideId Speaker(GuideId id);

/// The guide's round avatar at `size` pixels.
QPixmap Avatar(GuideId id, int size);

/// The resource path of the guide's avatar, for rich text.
QString AvatarPath(GuideId id);

/// A speech card: the guide's avatar (MechaRidds beside a pet) with `text`. When the guides are
/// off, just the text.
QWidget* MakeGuideBanner(GuideId id, const QString& text, QWidget* parent);

} // namespace Satronho
