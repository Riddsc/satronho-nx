// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <random>

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegion>
#include <QToolButton>
#include <QVBoxLayout>

#include "qt_common/config/uisettings.h"
#include "yuzu/satronho/dock.h"
#include "yuzu/satronho/knowledge.h"
#include "yuzu/satronho/novedades.h"

namespace Satronho {
namespace {

constexpr int BubbleWidth = 320;
constexpr int RightMargin = 24;
constexpr int BottomMargin = 10;

constexpr const char* DockStyle = R"(
QWidget#satronhoDock { background: transparent; }
QFrame#satronhoDockBubble { background: #211d35; border: 1px solid #3a3360; border-radius: 14px; }
QFrame#satronhoDockBubble QLabel { color: #ebe8f5; background: transparent; }
QFrame#satronhoDockBubble QPushButton { background: #2b2550; color: #ffffff; border: none;
                                        border-radius: 8px; padding: 6px 10px; }
QFrame#satronhoDockBubble QPushButton:hover { background: #3a3270; }
QFrame#satronhoDockBubble QPushButton#satronhoDockMain { background: #4fd1c5; color: #0e1a1a;
                                                         font-weight: 700; }
QFrame#satronhoDockBubble QPushButton#satronhoDockMain:hover { background: #6fe0d6; }
QFrame#satronhoDockBubble QPushButton#satronhoDockClose { background: transparent;
                                                          color: #a9a4c2; padding: 0px 6px; }
QToolButton#satronhoDockAvatar { background: transparent; border: none; padding: 0px; }
)";

QString Tr(const char* text) {
    return QCoreApplication::translate("Satronho", text);
}

/// The start of an answer: whole sentences up to about two hundred characters.
QString ShortAnswer(const QString& answer) {
    constexpr int Limit = 220;
    if (answer.size() <= Limit) {
        return answer;
    }
    int cut = -1;
    for (int at = answer.indexOf(QStringLiteral(". ")); at >= 0 && at < Limit;
         at = answer.indexOf(QStringLiteral(". "), at + 1)) {
        cut = at + 1;
    }
    if (cut >= 60) {
        return answer.left(cut);
    }
    const int space = answer.lastIndexOf(QLatin1Char(' '), Limit - 1);
    return answer.left(space > 0 ? space : Limit) + QStringLiteral("…");
}

QToolButton* MakeAvatarButton(QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("satronhoDockAvatar"));
    button->setAutoRaise(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setContextMenuPolicy(Qt::CustomContextMenu);
    return button;
}

} // Anonymous namespace

GuideDock::GuideDock(QWidget* host_, OpenHub open_hub_, std::function<void()> save_config_)
    : QWidget(host_), host{host_}, open_hub{std::move(open_hub_)},
      save_config{std::move(save_config_)} {
    setObjectName(QStringLiteral("satronhoDock"));
    setAttribute(Qt::WA_NoSystemBackground);
    setStyleSheet(QString::fromLatin1(DockStyle));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    bubble = new QFrame(this);
    bubble->setObjectName(QStringLiteral("satronhoDockBubble"));
    bubble->setFixedWidth(BubbleWidth);
    auto* bubble_layout = new QVBoxLayout(bubble);
    bubble_layout->setContentsMargins(14, 10, 10, 12);
    bubble_layout->setSpacing(8);

    auto* top = new QHBoxLayout;
    bubble_text = new QLabel(bubble);
    bubble_text->setWordWrap(true);
    bubble_text->setTextFormat(Qt::RichText);
    top->addWidget(bubble_text, 1);
    auto* close_button = new QPushButton(QStringLiteral("×"), bubble);
    close_button->setObjectName(QStringLiteral("satronhoDockClose"));
    close_button->setToolTip(Tr("Cerrar"));
    close_button->setCursor(Qt::PointingHandCursor);
    top->addWidget(close_button, 0, Qt::AlignTop);
    bubble_layout->addLayout(top);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    more_button = new QPushButton(Tr("Otro consejo"), bubble);
    more_button->setCursor(Qt::PointingHandCursor);
    action_button = new QPushButton(bubble);
    action_button->setObjectName(QStringLiteral("satronhoDockMain"));
    action_button->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(more_button);
    buttons->addWidget(action_button);
    bubble_layout->addLayout(buttons);
    layout->addWidget(bubble, 0, Qt::AlignRight);

    auto* avatars = new QHBoxLayout;
    avatars->setSpacing(0);
    avatars->addStretch(1);
    leader_button = MakeAvatarButton(this);
    pet_button = MakeAvatarButton(this);
    avatars->addWidget(leader_button, 0, Qt::AlignBottom);
    avatars->addWidget(pet_button, 0, Qt::AlignBottom);
    layout->addLayout(avatars);

    const GuideId leader = GuideId::MechaRidds;
    const int leader_size = GetGuide(leader).avatar_size * 3 / 4;
    leader_button->setIcon(QIcon(Avatar(leader, leader_size)));
    leader_button->setIconSize(QSize(leader_size, leader_size));

    connect(close_button, &QPushButton::clicked, this, [this] { Collapse(); });
    connect(more_button, &QPushButton::clicked, this, [this] { NextTip(); });
    connect(action_button, &QPushButton::clicked, this, [this] {
        const auto run = action;
        Collapse();
        if (run) {
            run();
        }
    });
    for (QToolButton* button : {leader_button, pet_button}) {
        connect(button, &QToolButton::clicked, this, [this] {
            if (bubble->isVisible()) {
                Collapse();
            } else {
                Expand();
            }
        });
        connect(button, &QToolButton::customContextMenuRequested, this,
                [this, button](const QPoint& pos) { ShowMenu(button->mapToGlobal(pos)); });
    }

    for (const Topic& topic : Topics()) {
        if (topic.guide != QLatin1String(GetGuide(GuideId::MechaRidds).key)) {
            tips.push_back(&topic);
        }
    }
    std::shuffle(tips.begin(), tips.end(), std::mt19937{QRandomGenerator::global()->generate()});
    if (!tips.empty()) {
        SetPet(GuideFromKey(tips.front()->guide));
    } else {
        SetPet(GuideId::Samy);
    }

    bubble->hide();
    host->installEventFilter(this);
    Refresh();
}

GuideDock::~GuideDock() = default;

bool GuideDock::Available() const {
    return GuidesEnabled() && UISettings::values.satronho_bubble.GetValue();
}

void GuideDock::Refresh() {
    if (!Available()) {
        hide();
        return;
    }
    if (!GuideEnabled(pet)) {
        SetPet(GuideId::MechaRidds);
    }
    show();
    Place();
}

void GuideDock::SetPet(GuideId guide) {
    pet = guide;
    if (guide == GuideId::MechaRidds) {
        pet_button->hide();
        leader_button->setToolTip(Tr("MechaRidds: haz clic y te doy un consejo"));
        return;
    }
    const Guide& info = GetGuide(guide);
    pet_button->setIcon(QIcon(Avatar(guide, info.avatar_size)));
    pet_button->setIconSize(QSize(info.avatar_size, info.avatar_size));
    const QString tooltip = Tr("MechaRidds y %1: haz clic y te damos un consejo").arg(info.name);
    leader_button->setToolTip(tooltip);
    pet_button->setToolTip(tooltip);
    pet_button->show();
}

void GuideDock::Expand() {
    NextTip();
}

void GuideDock::Collapse() {
    action = nullptr;
    bubble->hide();
    Place();
}

void GuideDock::NextTip() {
    const Topic* tip = nullptr;
    for (size_t tries = 0; tries < tips.size(); ++tries) {
        const Topic* candidate = tips[next_tip];
        next_tip = (next_tip + 1) % tips.size();
        if (GuideEnabled(GuideFromKey(candidate->guide))) {
            tip = candidate;
            break;
        }
    }
    if (tip == nullptr) {
        Speak(GuideId::MechaRidds,
              Tr("¿Tienes alguna duda sobre el emulador? Pregúntame lo que quieras."),
              Tr("Preguntar"), [this] { open_hub(Hub::Page::Preguntas, QString()); }, false);
        return;
    }
    const QString id = tip->id;
    Speak(GuideFromKey(tip->guide),
          QStringLiteral("<b>%1</b><br>%2")
              .arg(tip->question.toHtmlEscaped(), ShortAnswer(tip->answer).toHtmlEscaped()),
          Tr("Cuéntame más"), [this, id] { open_hub(Hub::Page::Preguntas, id); }, true);
}

void GuideDock::Speak(GuideId guide, const QString& html, const QString& action_text,
                      std::function<void()> action_, bool offer_more_tips) {
    const GuideId speaker = Speaker(guide);
    SetPet(speaker);
    const Guide& info = GetGuide(speaker);
    bubble_text->setText(QStringLiteral("<b style=\"color:%1\">%2</b><br>%3")
                             .arg(info.color.name(), info.name, html));
    action = std::move(action_);
    action_button->setText(action_text);
    action_button->setVisible(action != nullptr);
    more_button->setVisible(offer_more_tips && !tips.empty());
    bubble->show();
    Place();
}

void GuideDock::AnnounceNovedades(const QString& build_tag) {
    QString title;
    if (const BuildNotes* notes = NotesFor(build_tag)) {
        title = notes->title;
    }
    const QString build = QString(build_tag).replace(QStringLiteral("build-"), Tr("build "));
    QString text = Tr("¡Estás estrenando la %1!").arg(build.toHtmlEscaped());
    if (!title.isEmpty()) {
        text += QStringLiteral("<br>%1").arg(title.toHtmlEscaped());
    }
    Speak(GuideId::Juana, text, Tr("Ver lo nuevo"),
          [this] { open_hub(Hub::Page::Novedades, QString()); }, false);
}

void GuideDock::Place() {
    if (isHidden()) {
        return;
    }
    adjustSize();
    // Lay the children out now, also while hidden, so the mask below matches them.
    layout()->setGeometry(rect());
    // Stay above the filter bar at the bottom of the game list.
    int bottom = host->height();
    for (QObject* child : host->children()) {
        auto* widget = qobject_cast<QWidget*>(child);
        if (widget == nullptr || widget == this || widget->isHidden()) {
            continue;
        }
        const QRect geometry = widget->geometry();
        if (geometry.bottom() >= host->height() - 2 && geometry.height() < 80 &&
            geometry.width() > host->width() / 2) {
            bottom = std::min(bottom, geometry.top());
        }
    }
    move(host->width() - width() - RightMargin, bottom - height() - BottomMargin);

    // Only the bubble and the avatars take the mouse; the rest lets clicks reach the list.
    QRegion region;
    if (!bubble->isHidden()) {
        region += bubble->geometry();
    }
    region += QRegion(leader_button->geometry(), QRegion::Ellipse);
    if (!pet_button->isHidden()) {
        region += QRegion(pet_button->geometry(), QRegion::Ellipse);
    }
    setMask(region);
    raise();
}

void GuideDock::ShowMenu(const QPoint& global_pos) {
    QMenu menu(this);
    connect(menu.addAction(Tr("Pregúntale a los guías")), &QAction::triggered, this,
            [this] { open_hub(Hub::Page::Preguntas, QString()); });
    connect(menu.addAction(Tr("Novedades")), &QAction::triggered, this,
            [this] { open_hub(Hub::Page::Novedades, QString()); });
    menu.addSeparator();
    connect(menu.addAction(Tr("Ocultar la burbuja")), &QAction::triggered, this, [this] {
        UISettings::values.satronho_bubble.SetValue(false);
        if (save_config) {
            save_config();
        }
        Refresh();
    });
    menu.exec(global_pos);
}

bool GuideDock::eventFilter(QObject* object, QEvent* event) {
    if (object == host) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::ChildAdded:
        case QEvent::LayoutRequest:
            if (!isHidden()) {
                Place();
            }
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(object, event);
}

} // namespace Satronho
