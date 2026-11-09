// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

#include "qt_common/config/uisettings.h"
#include "yuzu/satronho/guides.h"

namespace Satronho {
namespace {

QString Tr(const char* text) {
    return QCoreApplication::translate("Satronho", text);
}

Settings::Setting<bool>* PetSetting(GuideId id) {
    switch (id) {
    case GuideId::Samy:
        return &UISettings::values.satronho_guide_samy;
    case GuideId::Troya:
        return &UISettings::values.satronho_guide_troya;
    case GuideId::Nono:
        return &UISettings::values.satronho_guide_nono;
    case GuideId::Juana:
        return &UISettings::values.satronho_guide_juana;
    case GuideId::MechaRidds:
        break;
    }
    return nullptr;
}

} // Anonymous namespace

const std::array<Guide, 5>& Guides() {
    static const std::array<Guide, 5> guides{{
        {GuideId::MechaRidds, "mecharidds", QStringLiteral("MechaRidds"), Tr("Te acompaña en todo"),
         Tr("El protagonista. Conoce cada rincón de Satronho y responde por cualquier mascota "
            "que esté apagada."),
         QColor(0x74, 0xd1, 0x4c), 60},
        {GuideId::Samy, "samy", QStringLiteral("Samy"), Tr("Funciones del emulador"),
         Tr("Pitbull terrier café con blanco. Te explica los juegos, los complementos, los "
            "controles, los atajos y las carpetas."),
         QColor(0xff, 0xb2, 0x3e), 54},
        {GuideId::Troya, "troya", QStringLiteral("Troya"), Tr("Gráficos e imagen"),
         Tr("Pitbull malinois de cejitas amarillas y gafitas. Sabe de resolución, filtros, "
            "antialiasing y de la niebla y la exposición de Ocarina."),
         QColor(0x9b, 0x8c, 0xff), 58},
        {GuideId::Nono, "nono", QStringLiteral("Ñoño"), Tr("Configuraciones y rendimiento"),
         Tr("Labrabull blanco con negro, el más grande. Revisa tu equipo y te dice qué "
            "configuración le conviene."),
         QColor(0x4f, 0xd1, 0xc5), 64},
        {GuideId::Juana, "juana", QStringLiteral("Juana"), Tr("Novedades y actualizaciones"),
         Tr("La viejita criolla negra de canas y moñito, la más pequeña. Te avisa cuando hay una "
            "build nueva y te cuenta qué trae."),
         QColor(0xff, 0x8f, 0xb5), 40},
    }};
    return guides;
}

const Guide& GetGuide(GuideId id) {
    return Guides()[static_cast<size_t>(id)];
}

GuideId GuideFromKey(const QString& key) {
    for (const Guide& guide : Guides()) {
        if (key == QLatin1String(guide.key)) {
            return guide.id;
        }
    }
    return GuideId::MechaRidds;
}

bool GuidesEnabled() {
    return UISettings::values.satronho_guides.GetValue();
}

bool GuideEnabled(GuideId id) {
    if (!GuidesEnabled()) {
        return false;
    }
    const auto* setting = PetSetting(id);
    return setting == nullptr || setting->GetValue();
}

void SetGuideEnabled(GuideId id, bool enabled) {
    if (auto* setting = PetSetting(id)) {
        setting->SetValue(enabled);
    } else {
        UISettings::values.satronho_guides.SetValue(enabled);
    }
}

GuideId Speaker(GuideId id) {
    return GuideEnabled(id) ? id : GuideId::MechaRidds;
}

QString AvatarPath(GuideId id) {
    return QStringLiteral(":/satronho/guias/%1.png").arg(QLatin1String(GetGuide(id).key));
}

QPixmap Avatar(GuideId id, int size) {
    const QPixmap pixmap(AvatarPath(id));
    if (pixmap.isNull()) {
        return pixmap;
    }
    return pixmap.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QWidget* MakeGuideBanner(GuideId id, const QString& text, QWidget* parent) {
    auto* banner = new QWidget(parent);
    banner->setObjectName(QStringLiteral("satronhoBanner"));
    auto* layout = new QHBoxLayout(banner);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    if (GuidesEnabled()) {
        const GuideId speaker = Speaker(id);
        const auto add_avatar = [&](GuideId guide) {
            auto* avatar = new QLabel(banner);
            const int size = GetGuide(guide).avatar_size;
            avatar->setPixmap(Avatar(guide, size));
            avatar->setFixedSize(size, size);
            avatar->setToolTip(GetGuide(guide).name);
            layout->addWidget(avatar, 0, Qt::AlignBottom);
        };
        if (speaker != GuideId::MechaRidds) {
            add_avatar(GuideId::MechaRidds);
        }
        add_avatar(speaker);
        auto* bubble = new QLabel(
            QStringLiteral("<b style=\"color:%1\">%2</b><br>%3")
                .arg(GetGuide(speaker).color.name(), GetGuide(speaker).name, text),
            banner);
        bubble->setWordWrap(true);
        bubble->setTextFormat(Qt::RichText);
        bubble->setObjectName(QStringLiteral("satronhoBubble"));
        layout->addWidget(bubble, 1);
    } else {
        auto* label = new QLabel(text, banner);
        label->setWordWrap(true);
        label->setTextFormat(Qt::RichText);
        layout->addWidget(label, 1);
    }
    return banner;
}

} // namespace Satronho
