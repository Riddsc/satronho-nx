// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <vector>

#include <QString>
#include <QWidget>

#include "yuzu/satronho/guides.h"
#include "yuzu/satronho/hub.h"

class QFrame;
class QLabel;
class QPushButton;
class QToolButton;

namespace Satronho {

struct Topic;

/// The guides' bubble in the bottom right corner of the game list: MechaRidds and a pet, who
/// give a tip when clicked and announce what a new build brings. It lives on the game list, so it
/// is out of the way while a game runs.
class GuideDock : public QWidget {
public:
    /// Opens the Satronho window at `page`; for Preguntas, `topic` is the topic to show.
    using OpenHub = std::function<void(Hub::Page page, const QString& topic)>;

    GuideDock(QWidget* host, OpenHub open_hub, std::function<void()> save_config);
    ~GuideDock() override;

    /// Shows or hides the bubble after the guide settings.
    void Refresh();

    /// Whether the bubble is on and can speak.
    bool Available() const;

    /// The first start of a newly installed build: a pet tells what it brings.
    void AnnounceNovedades(const QString& build_tag);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void Expand();
    void Collapse();
    void NextTip();
    void Speak(GuideId guide, const QString& html, const QString& action_text,
               std::function<void()> action, bool offer_more_tips);
    void SetPet(GuideId guide);
    void Place();
    void ShowMenu(const QPoint& global_pos);

    QWidget* host;
    OpenHub open_hub;
    std::function<void()> save_config;

    QFrame* bubble{};
    QLabel* bubble_text{};
    QPushButton* action_button{};
    QPushButton* more_button{};
    QToolButton* leader_button{};
    QToolButton* pet_button{};

    GuideId pet{GuideId::Samy};
    std::function<void()> action;
    std::vector<const Topic*> tips;
    size_t next_tip{};
};

} // namespace Satronho
