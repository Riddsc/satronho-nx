// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <vector>

#include <QDialog>

#include "yuzu/satronho/guides.h"
#include "yuzu/satronho/presets.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;
class QTextBrowser;
class QVBoxLayout;

namespace VkDeviceInfo {
class Record;
}

namespace Satronho {

struct Topic;

/// The "Satronho" window: what's new, suggested settings, questions to the guides, and the
/// guides themselves.
class Hub : public QDialog {
    Q_OBJECT

public:
    enum class Page { Novedades, Sugeridas, Preguntas, Guias };

    Hub(const std::vector<VkDeviceInfo::Record>& records, bool emulation_running,
        std::function<void()> save_config, Page page, QWidget* parent);
    ~Hub() override;

    /// Opens the window at `page`.
    static void Open(const std::vector<VkDeviceInfo::Record>& records, bool emulation_running,
                     std::function<void()> save_config, Page page, QWidget* parent);

private:
    QWidget* BuildNovedades();
    QWidget* BuildSugeridas();
    QWidget* BuildPreguntas();
    QWidget* BuildGuias();

    void SetBanner(QVBoxLayout* layout, QWidget*& banner, GuideId guide, const QString& text);
    void RefreshBanners();
    void RefreshSuggestion();
    void ApplySuggestionClicked();

    void Ask(const QString& text);
    void ShowTopic(const Topic& topic);
    void ShowArea(GuideId guide);
    void Say(GuideId guide, const QString& html);
    void SayUser(const QString& text);
    void Greet();

    std::function<void()> save_config;
    bool emulation_running;
    Hardware hardware;
    Suggestion suggestion;

    QTabWidget* tabs{};

    QVBoxLayout* novedades_layout{};
    QWidget* novedades_banner{};

    QVBoxLayout* sugeridas_layout{};
    QWidget* sugeridas_banner{};
    QString sugeridas_message;
    QLabel* hardware_label{};
    QTableWidget* changes_table{};
    QLabel* notes_label{};
    QCheckBox* per_game_box{};
    QPushButton* apply_button{};

    QTextBrowser* chat{};
    QLineEdit* input{};
    QString chat_html;

    QVBoxLayout* guias_layout{};
    QWidget* guias_banner{};
};

/// Opens the "what's new" page the first time a newly installed build starts.
void ShowNovedadesIfNew(const std::vector<VkDeviceInfo::Record>& records,
                        std::function<void()> save_config, QWidget* parent);

} // namespace Satronho
