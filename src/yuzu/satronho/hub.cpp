// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCheckBox>
#include <QCoreApplication>
#include <QIcon>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include "qt_common/util/vk.h"
#include "yuzu/satronho/hub.h"
#include "yuzu/satronho/knowledge.h"
#include "yuzu/satronho/novedades.h"

namespace Satronho {
namespace {

constexpr const char* HubStyle = R"(
QDialog#satronhoHub { background-color: #15131f; }
QDialog#satronhoHub QLabel, QDialog#satronhoHub QCheckBox { color: #ebe8f5; }
QTabWidget::pane { border: 1px solid #2c2842; border-radius: 10px; background: #1b1829; top: -1px; }
QTabBar::tab { background: #1b1829; color: #a9a4c2; padding: 9px 18px; border: 1px solid #2c2842;
               border-bottom: none; border-top-left-radius: 9px; border-top-right-radius: 9px;
               margin-right: 4px; }
QTabBar::tab:selected { background: #262140; color: #ffffff; }
QTextBrowser, QTableWidget { background: #1b1829; color: #ebe8f5; border: none;
                             selection-background-color: #3a3360; }
QHeaderView::section { background: #232036; color: #a9a4c2; border: none; padding: 6px; }
QTableWidget { gridline-color: #2c2842; }
QLineEdit { background: #232036; color: #ffffff; border: 1px solid #3a3360; border-radius: 9px;
            padding: 8px 10px; }
QPushButton { background: #2b2550; color: #ffffff; border: none; border-radius: 9px;
              padding: 8px 14px; }
QPushButton:hover { background: #3a3270; }
QPushButton:disabled { background: #24213a; color: #6e6a86; }
QPushButton#satronhoPrimary { background: #4fd1c5; color: #0e1a1a; font-weight: 700; }
QPushButton#satronhoPrimary:hover { background: #6fe0d6; }
QPushButton#satronhoPrimary:disabled { background: #24213a; color: #6e6a86; }
QWidget#satronhoBanner { background: #211d35; border-radius: 12px; }
QFrame#satronhoCard { background: #211d35; border-radius: 12px; }
)";

QString Tr(const char* text) {
    return QCoreApplication::translate("Satronho", text);
}

QLabel* Heading(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() * 1.25);
    font.setBold(true);
    label->setFont(font);
    return label;
}

} // Anonymous namespace

Hub::Hub(const std::vector<VkDeviceInfo::Record>& records, bool emulation_running_,
         std::function<void()> save_config_, Page page, QWidget* parent)
    : QDialog(parent), save_config{std::move(save_config_)},
      emulation_running{emulation_running_}, hardware{DetectHardware(records)} {
    setObjectName(QStringLiteral("satronhoHub"));
    setWindowTitle(QStringLiteral("Satronho"));
    setStyleSheet(QString::fromLatin1(HubStyle));
    resize(900, 680);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    tabs = new QTabWidget(this);
    tabs->addTab(BuildNovedades(), Tr("Novedades"));
    tabs->addTab(BuildSugeridas(), Tr("Configuraciones sugeridas"));
    tabs->addTab(BuildPreguntas(), Tr("Pregúntale a los guías"));
    tabs->addTab(BuildGuias(), Tr("Guías"));
    layout->addWidget(tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    tabs->setCurrentIndex(static_cast<int>(page));
}

Hub::~Hub() = default;

void Hub::Open(const std::vector<VkDeviceInfo::Record>& records, bool emulation_running,
               std::function<void()> save_config, Page page, QWidget* parent) {
    Hub hub(records, emulation_running, std::move(save_config), page, parent);
    hub.exec();
}

void Hub::SetBanner(QVBoxLayout* layout, QWidget*& banner, GuideId guide, const QString& text) {
    QWidget* replacement = MakeGuideBanner(guide, text, layout->parentWidget());
    if (banner != nullptr) {
        layout->replaceWidget(banner, replacement);
        banner->deleteLater();
    } else {
        layout->insertWidget(0, replacement);
    }
    banner = replacement;
}

void Hub::RefreshBanners() {
    const QString tag = CurrentBuildTag();
    SetBanner(novedades_layout, novedades_banner, GuideId::Juana,
              tag.isEmpty() || NotesFor(tag) == nullptr
                  ? Tr("Esto es lo nuevo de cada build. Nuevo es algo que no existía, Mejora algo "
                       "que funciona mejor y Arreglo algo que fallaba y ya no.")
                  : Tr("¡Hola! Estás estrenando la %1. Esto es lo que trae, y más abajo, lo de las "
                       "builds anteriores.")
                        .arg(QString(tag).replace(QStringLiteral("build-"), Tr("build "))));
    SetBanner(sugeridas_layout, sugeridas_banner, GuideId::Nono,
              sugeridas_message.isEmpty() ? suggestion.summary : sugeridas_message);
    SetBanner(guias_layout, guias_banner, GuideId::MechaRidds,
              Tr("Somos tus guías. Cada mascota se encarga de un tema y yo los acompaño a todos. "
                 "Puedes apagarnos cuando quieras; si apagas a una mascota, respondo yo por ella."));
}

QWidget* Hub::BuildNovedades() {
    auto* page = new QWidget(this);
    novedades_layout = new QVBoxLayout(page);
    novedades_layout->setContentsMargins(14, 14, 14, 14);
    novedades_layout->setSpacing(12);
    auto* browser = new QTextBrowser(page);
    browser->setOpenExternalLinks(true);
    browser->setHtml(AllNotes().empty()
                         ? Tr("No encontré la lista de novedades de esta build.")
                         : NotesHtml(AllNotes()));
    novedades_layout->addWidget(browser, 1);
    SetBanner(novedades_layout, novedades_banner, GuideId::Juana, QString());
    return page;
}

QWidget* Hub::BuildSugeridas() {
    auto* page = new QWidget(this);
    sugeridas_layout = new QVBoxLayout(page);
    sugeridas_layout->setContentsMargins(14, 14, 14, 14);
    sugeridas_layout->setSpacing(10);

    hardware_label = new QLabel(page);
    hardware_label->setTextFormat(Qt::RichText);
    hardware_label->setWordWrap(true);
    sugeridas_layout->addWidget(hardware_label);

    changes_table = new QTableWidget(page);
    changes_table->setColumnCount(5);
    changes_table->setHorizontalHeaderLabels(
        {Tr("Opción"), Tr("Dónde está"), Tr("Ahora"), Tr("Sugerido"), Tr("Por qué")});
    changes_table->verticalHeader()->setVisible(false);
    changes_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    changes_table->setSelectionMode(QAbstractItemView::NoSelection);
    changes_table->setWordWrap(true);
    changes_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    sugeridas_layout->addWidget(changes_table, 1);

    notes_label = new QLabel(page);
    notes_label->setTextFormat(Qt::RichText);
    notes_label->setWordWrap(true);
    sugeridas_layout->addWidget(notes_label);

    per_game_box = new QCheckBox(
        Tr("Que los juegos con configuración propia usen también la general en estas opciones"),
        page);
    per_game_box->setChecked(true);
    per_game_box->setToolTip(
        Tr("Si un juego tiene su propio valor en alguna de estas opciones, ese manda sobre la "
           "configuración general. Con esta casilla, esas opciones del juego pasan a usar la "
           "general. Sus demás ajustes no se tocan."));
    sugeridas_layout->addWidget(per_game_box);

    auto* buttons = new QHBoxLayout;
    auto* guide_button = new QPushButton(Tr("Ver la guía por generaciones"), page);
    guide_button->setCheckable(true);
    apply_button = new QPushButton(Tr("Aplícalo por mí"), page);
    apply_button->setObjectName(QStringLiteral("satronhoPrimary"));
    buttons->addWidget(guide_button);
    buttons->addStretch(1);
    buttons->addWidget(apply_button);
    sugeridas_layout->addLayout(buttons);

    auto* guide = new QTextBrowser(page);
    guide->setHtml(GenerationGuideHtml());
    guide->setMinimumHeight(220);
    guide->setVisible(false);
    sugeridas_layout->addWidget(guide);
    connect(guide_button, &QPushButton::toggled, guide, &QWidget::setVisible);
    connect(apply_button, &QPushButton::clicked, this, &Hub::ApplySuggestionClicked);

    RefreshSuggestion();
    SetBanner(sugeridas_layout, sugeridas_banner, GuideId::Nono, suggestion.summary);
    return page;
}

void Hub::RefreshSuggestion() {
    suggestion = Suggest(hardware);

    const auto row = [](const QString& name, const QString& value) {
        return QStringLiteral("<tr><td style=\"color:#a9a4c2; padding-right:14px\">%1</td>"
                              "<td>%2</td></tr>")
            .arg(name, value.toHtmlEscaped());
    };
    QString info = QStringLiteral("<table cellspacing=\"0\" cellpadding=\"2\">");
    info += row(Tr("Tarjeta gráfica"),
                hardware.gpu_name.isEmpty() ? Tr("No encontrada") : hardware.gpu_name);
    info += row(Tr("Familia"), suggestion.family_name);
    if (hardware.vram_bytes > 0) {
        info += row(Tr("Memoria de video"),
                    QStringLiteral("%1 GB").arg(static_cast<double>(hardware.vram_bytes) /
                                                    static_cast<double>(1ULL << 30),
                                                0, 'f', 1));
    }
    if (!hardware.cpu_name.isEmpty()) {
        info += row(Tr("Procesador"),
                    Tr("%1 (%2 hilos)").arg(hardware.cpu_name).arg(hardware.cpu_threads));
    }
    if (hardware.ram_bytes > 0) {
        info += row(Tr("RAM"), QStringLiteral("%1 GB").arg(static_cast<double>(hardware.ram_bytes) /
                                                               static_cast<double>(1ULL << 30),
                                                           0, 'f', 0));
    }
    info += QStringLiteral("</table>");
    hardware_label->setText(info);

    int pending = 0;
    changes_table->setRowCount(static_cast<int>(suggestion.changes.size()));
    for (int i = 0; i < static_cast<int>(suggestion.changes.size()); ++i) {
        const SettingChange& change = suggestion.changes[static_cast<size_t>(i)];
        const QStringList cells{change.label, change.location, change.current, change.suggested,
                                change.reason};
        for (int column = 0; column < cells.size(); ++column) {
            auto* item = new QTableWidgetItem(cells[column]);
            item->setToolTip(change.reason);
            if (column == 3) {
                item->setForeground(change.differs ? QColor(0x4f, 0xd1, 0xc5)
                                                   : QColor(0x8d, 0x88, 0xa6));
                QFont font = item->font();
                font.setBold(change.differs);
                item->setFont(font);
            }
            changes_table->setItem(i, column, item);
        }
        pending += change.differs ? 1 : 0;
    }
    changes_table->resizeColumnsToContents();
    changes_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    changes_table->resizeRowsToContents();

    QString notes;
    for (const QString& note : suggestion.notes) {
        notes += QStringLiteral("• %1<br>").arg(note.toHtmlEscaped());
    }
    notes_label->setText(notes);
    notes_label->setVisible(!notes.isEmpty());

    if (emulation_running) {
        apply_button->setEnabled(false);
        apply_button->setToolTip(Tr("Cierra el juego para aplicar la configuración."));
    } else {
        apply_button->setEnabled(pending > 0);
        apply_button->setToolTip(pending > 0 ? QString()
                                             : Tr("Ya tienes la configuración sugerida."));
    }
    apply_button->setText(pending > 0 ? Tr("Aplícalo por mí (%1 cambios)").arg(pending)
                                      : Tr("Ya está aplicada"));
}

void Hub::ApplySuggestionClicked() {
    if (emulation_running) {
        return;
    }
    const int games = ApplySuggestion(suggestion, per_game_box->isChecked());
    if (save_config) {
        save_config();
    }
    sugeridas_message =
        games > 0 ? Tr("¡Listo! Ya quedó todo configurado, y %1 juegos con configuración propia "
                       "usan ahora la general en estas opciones. Si quieres volver a algo, "
                       "cámbialo en «Emulation → Configure».")
                        .arg(games)
                  : Tr("¡Listo! Ya quedó todo configurado. Si quieres volver a algo, cámbialo en "
                       "«Emulation → Configure».");
    RefreshSuggestion();
    SetBanner(sugeridas_layout, sugeridas_banner, GuideId::Nono, sugeridas_message);
}

QWidget* Hub::BuildPreguntas() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    chat = new QTextBrowser(page);
    chat->setOpenLinks(false);
    connect(chat, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        if (url.scheme() == QStringLiteral("tema")) {
            if (const Topic* topic = FindTopic(url.path())) {
                SayUser(topic->question);
                ShowTopic(*topic);
            }
        } else if (url.scheme() == QStringLiteral("area")) {
            ShowArea(GuideFromKey(url.path()));
        }
    });
    layout->addWidget(chat, 1);

    auto* areas = new QHBoxLayout;
    areas->addWidget(new QLabel(Tr("Temas:"), page));
    for (const Guide& guide : Guides()) {
        auto* button = new QPushButton(guide.role, page);
        button->setIcon(QIcon(Avatar(guide.id, 24)));
        const GuideId id = guide.id;
        connect(button, &QPushButton::clicked, this, [this, id] { ShowArea(id); });
        areas->addWidget(button);
    }
    areas->addStretch(1);
    layout->addLayout(areas);

    auto* row = new QHBoxLayout;
    input = new QLineEdit(page);
    input->setPlaceholderText(
        Tr("Escribe tu pregunta, por ejemplo: ¿qué hace la resolución? o ¿cómo actualizo?"));
    auto* send = new QPushButton(Tr("Preguntar"), page);
    send->setObjectName(QStringLiteral("satronhoPrimary"));
    row->addWidget(input, 1);
    row->addWidget(send);
    layout->addLayout(row);
    const auto submit = [this] {
        const QString text = input->text().trimmed();
        if (!text.isEmpty()) {
            input->clear();
            Ask(text);
        }
    };
    connect(send, &QPushButton::clicked, this, submit);
    connect(input, &QLineEdit::returnPressed, this, submit);

    Greet();
    return page;
}

void Hub::Say(GuideId guide, const QString& html) {
    const GuideId speaker = Speaker(guide);
    QString name;
    QString avatar;
    if (GuidesEnabled()) {
        const Guide& info = GetGuide(speaker);
        name = QStringLiteral("<b style=\"color:%1\">%2</b>").arg(info.color.name(), info.name);
        if (speaker != guide) {
            name += QStringLiteral(" <span style=\"color:#8d88a6\">%1</span>")
                        .arg(Tr("(por %1)").arg(GetGuide(guide).name));
        }
        avatar = QStringLiteral("<img src=\"%1\" width=\"%2\" height=\"%2\">")
                     .arg(AvatarPath(speaker))
                     .arg(info.avatar_size * 3 / 4);
    } else {
        name = QStringLiteral("<b>Satronho</b>");
    }
    chat_html += QStringLiteral(
                     "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"4\"><tr>"
                     "<td width=\"56\" valign=\"top\" align=\"center\">%1</td>"
                     "<td valign=\"top\" bgcolor=\"#24203a\" style=\"padding:10px\">%2<br>%3</td>"
                     "<td width=\"60\"></td></tr></table><br>")
                     .arg(avatar, name, html);
    chat->setHtml(chat_html);
    chat->verticalScrollBar()->setValue(chat->verticalScrollBar()->maximum());
}

void Hub::SayUser(const QString& text) {
    chat_html += QStringLiteral(
                     "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"4\"><tr>"
                     "<td width=\"120\"></td>"
                     "<td valign=\"top\" align=\"right\" bgcolor=\"#2f2a52\" "
                     "style=\"padding:10px\">%1</td></tr></table><br>")
                     .arg(text.toHtmlEscaped());
    chat->setHtml(chat_html);
}

void Hub::Greet() {
    Say(GuideId::MechaRidds,
        Tr("¡Hola! Pregúntanos lo que quieras sobre las funciones del emulador, las "
           "configuraciones o las actualizaciones. Escríbelo con tus palabras o elige un tema de "
           "abajo."));
}

void Hub::Ask(const QString& text) {
    SayUser(text);
    const Answer answer = Satronho::Ask(text);
    if (answer.topic != nullptr) {
        ShowTopic(*answer.topic);
        return;
    }
    if (!answer.others.empty()) {
        QString links;
        for (const Topic* topic : answer.others) {
            links += QStringLiteral("• <a href=\"tema:%1\" style=\"color:#4fd1c5\">%2</a><br>")
                         .arg(topic->id, topic->question.toHtmlEscaped());
        }
        Say(GuideFromKey(answer.others.front()->guide),
            Tr("No estoy seguro de haberte entendido. ¿Te refieres a alguna de estas?<br>%1")
                .arg(links));
        return;
    }
    QString areas;
    for (const Guide& guide : Guides()) {
        areas += QStringLiteral("• <a href=\"area:%1\" style=\"color:#4fd1c5\">%2</a><br>")
                     .arg(QLatin1String(guide.key), guide.role.toHtmlEscaped());
    }
    Say(GuideId::MechaRidds,
        Tr("Eso no lo sé. Sé de las funciones del emulador, de configuraciones y de "
           "actualizaciones. Prueba con otras palabras o mira uno de estos temas:<br>%1")
            .arg(areas));
}

void Hub::ShowTopic(const Topic& topic) {
    QString html = QStringLiteral("<b>%1</b><br>%2").arg(topic.question.toHtmlEscaped(),
                                                        topic.answer.toHtmlEscaped());
    QString related;
    for (const QString& id : topic.related) {
        if (const Topic* other = FindTopic(id)) {
            related += QStringLiteral("• <a href=\"tema:%1\" style=\"color:#4fd1c5\">%2</a><br>")
                           .arg(other->id, other->question.toHtmlEscaped());
        }
    }
    if (!related.isEmpty()) {
        html += QStringLiteral("<br><br><span style=\"color:#a9a4c2\">%1</span><br>%2")
                    .arg(Tr("También te puede servir:"), related);
    }
    Say(GuideFromKey(topic.guide), html);
}

void Hub::ShowArea(GuideId guide) {
    const QLatin1String key(GetGuide(guide).key);
    QString links;
    for (const Topic& topic : Topics()) {
        if (topic.guide == key) {
            links += QStringLiteral("• <a href=\"tema:%1\" style=\"color:#4fd1c5\">%2</a><br>")
                         .arg(topic.id, topic.question.toHtmlEscaped());
        }
    }
    Say(guide, Tr("Esto es lo que te puedo contar de %1:<br>%2")
                   .arg(GetGuide(guide).role.toLower(), links));
}

QWidget* Hub::BuildGuias() {
    auto* page = new QWidget(this);
    guias_layout = new QVBoxLayout(page);
    guias_layout->setContentsMargins(14, 14, 14, 14);
    guias_layout->setSpacing(10);

    auto* all = new QCheckBox(Tr("Mostrar a los guías"), page);
    all->setChecked(GuidesEnabled());
    guias_layout->addWidget(all);

    auto* grid = new QGridLayout;
    grid->setSpacing(10);
    std::vector<QCheckBox*> pet_boxes;
    int index = 0;
    for (const Guide& guide : Guides()) {
        auto* card = new QFrame(page);
        card->setObjectName(QStringLiteral("satronhoCard"));
        auto* card_layout = new QHBoxLayout(card);
        card_layout->setContentsMargins(12, 12, 12, 12);
        auto* avatar = new QLabel(card);
        avatar->setPixmap(Avatar(guide.id, guide.avatar_size));
        avatar->setFixedSize(64, 64);
        avatar->setAlignment(Qt::AlignCenter);
        card_layout->addWidget(avatar);
        auto* text = new QLabel(QStringLiteral("<b style=\"color:%1; font-size:15px\">%2</b><br>"
                                               "<b>%3</b><br><span style=\"color:#a9a4c2\">%4"
                                               "</span>")
                                    .arg(guide.color.name(), guide.name, guide.role.toHtmlEscaped(),
                                         guide.about.toHtmlEscaped()),
                                card);
        text->setWordWrap(true);
        text->setTextFormat(Qt::RichText);
        card_layout->addWidget(text, 1);
        if (guide.id != GuideId::MechaRidds) {
            auto* box = new QCheckBox(Tr("Activo"), card);
            box->setChecked(GuideEnabled(guide.id) || !GuidesEnabled());
            box->setEnabled(GuidesEnabled());
            const GuideId id = guide.id;
            connect(box, &QCheckBox::toggled, this, [this, id](bool checked) {
                SetGuideEnabled(id, checked);
                if (save_config) {
                    save_config();
                }
                RefreshBanners();
            });
            card_layout->addWidget(box);
            pet_boxes.push_back(box);
        }
        grid->addWidget(card, index / 2, index % 2);
        ++index;
    }
    guias_layout->addLayout(grid);
    guias_layout->addStretch(1);

    connect(all, &QCheckBox::toggled, this, [this, pet_boxes](bool checked) {
        SetGuideEnabled(GuideId::MechaRidds, checked);
        for (QCheckBox* box : pet_boxes) {
            box->setEnabled(checked);
        }
        if (save_config) {
            save_config();
        }
        RefreshBanners();
    });

    RefreshBanners();
    return page;
}

void ShowNovedadesIfNew(const std::vector<VkDeviceInfo::Record>& records,
                        std::function<void()> save_config, QWidget* parent) {
    if (!HasUnseenNotes()) {
        return;
    }
    MarkNotesSeen();
    if (save_config) {
        save_config();
    }
    Hub::Open(records, false, std::move(save_config), Hub::Page::Novedades, parent);
}

} // namespace Satronho
