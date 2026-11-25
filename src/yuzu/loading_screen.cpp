// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QBuffer>
#include <QByteArray>
#include <QGraphicsOpacityEffect>
#include <QIODevice>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QLabel>
#include <QStyleOption>
#include "common/container/unordered_map.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/loader/loader.h"
#include "ui_loading_screen.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/shader_warm/shader_warm_builder.h"
#include "yuzu/loading_screen.h"
#include "yuzu/satronho/guides.h"

// Mingw seems to not have QMovie at all. If QMovie is missing then use a single frame instead of an
// showing the full animation
#if !YUZU_QT_MOVIE_MISSING
#include <QMovie>
#endif

constexpr char PROGRESSBAR_STYLE_PREPARE[] = R"(
QProgressBar {}
QProgressBar::chunk {})";

constexpr char PROGRESSBAR_STYLE_BUILD[] = R"(
QProgressBar {
  background-color: black;
  border: 2px solid white;
  border-radius: 4px;
  padding: 2px;
}
QProgressBar::chunk {
  background-color: #ff3c28;
  width: 1px;
})";

constexpr char SKIP_BUTTON_STYLE[] = R"(
QPushButton {
  background-color: black;
  color: white;
  border: 2px solid white;
  border-radius: 6px;
  padding: 6px 24px;
  font: 12pt "Arial";
}
QPushButton:disabled {
  color: gray;
  border-color: gray;
})";

constexpr char PROGRESSBAR_STYLE_COMPLETE[] = R"(
QProgressBar {
  background-color: #0ab9e6;
  border: 2px solid white;
  border-radius: 4px;
  padding: 2px;
}
QProgressBar::chunk {
  background-color: #ff3c28;
})";

LoadingScreen::LoadingScreen(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::LoadingScreen>()),
      previous_stage(VideoCore::LoadCallbackStage::Complete) {
    ui->setupUi(this);
    setMinimumSize(Layout::MinimumSize::Width, Layout::MinimumSize::Height);

    // Create a fade out effect to hide this loading screen widget.
    // When fading opacity, it will fade to the parent widgets background color, which is why we
    // create an internal widget named fade_widget that we use the effect on, while keeping the
    // loading screen widget's background color black. This way we can create a fade to black effect
    opacity_effect = new QGraphicsOpacityEffect(this);
    opacity_effect->setOpacity(1);
    ui->fade_parent->setGraphicsEffect(opacity_effect);
    fadeout_animation = std::make_unique<QPropertyAnimation>(opacity_effect, "opacity");
    fadeout_animation->setDuration(500);
    fadeout_animation->setStartValue(1);
    fadeout_animation->setEndValue(0);
    fadeout_animation->setEasingCurve(QEasingCurve::OutBack);

    // After the fade completes, hide the widget and reset the opacity
    connect(fadeout_animation.get(), &QPropertyAnimation::finished, [this] {
        hide();
        opacity_effect->setOpacity(1);
        emit Hidden();
    });
    connect(this, &LoadingScreen::LoadProgress, this, &LoadingScreen::OnLoadProgress,
            Qt::QueuedConnection);

    // Shown while shaders are precached: starts the game without waiting for the rest.
    skip_button = new QPushButton(tr("Skip"), this);
    skip_button->setStyleSheet(QString::fromUtf8(SKIP_BUTTON_STYLE));
    skip_button->setToolTip(tr("Start the game now. The shaders not precached yet are precached "
                               "on a later launch."));
    skip_button->hide();
    ui->verticalLayout->insertWidget(2, skip_button, 0, Qt::AlignHCenter | Qt::AlignTop);
    connect(skip_button, &QPushButton::clicked, this, [this] {
        VideoCommon::ShaderWarm::RequestSkipWarm();
        skip_button->setEnabled(false);
    });
    qRegisterMetaType<VideoCore::LoadCallbackStage>();

    tip_label = new QLabel(this);
    tip_label->setWordWrap(true);
    tip_label->setTextFormat(Qt::RichText);
    tip_label->setMaximumWidth(640);
    tip_label->setStyleSheet(QStringLiteral("QLabel { color: #B9B2CC; font-size: 10pt; "
                                            "background: transparent; }"));
    tip_label->hide();
    ui->verticalLayout->addWidget(tip_label, 0, Qt::AlignHCenter | Qt::AlignBottom);

    stage_translations = {
        {VideoCore::LoadCallbackStage::Prepare, tr("Loading...")},
        {VideoCore::LoadCallbackStage::Build, tr("Loading Shaders %1 / %2")},
        {VideoCore::LoadCallbackStage::Complete, tr("Launching...")},
        {VideoCore::LoadCallbackStage::Warm, tr("Precaching shaders %1 / %2")},
    };
    progressbar_style = {
        {VideoCore::LoadCallbackStage::Prepare, PROGRESSBAR_STYLE_PREPARE},
        {VideoCore::LoadCallbackStage::Build, PROGRESSBAR_STYLE_BUILD},
        {VideoCore::LoadCallbackStage::Complete, PROGRESSBAR_STYLE_COMPLETE},
        {VideoCore::LoadCallbackStage::Warm, PROGRESSBAR_STYLE_BUILD},
    };
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Prepare(Loader::AppLoader& loader) {
    std::vector<u8> buffer;
    if (loader.ReadBanner(buffer) == Loader::ResultStatus::Success) {
#ifdef YUZU_QT_MOVIE_MISSING
        QPixmap map;
        map.loadFromData(buffer.data(), buffer.size());
        ui->banner->setPixmap(map);
#else
        backing_mem = std::make_unique<QByteArray>(reinterpret_cast<char*>(buffer.data()),
                                                   static_cast<int>(buffer.size()));
        backing_buf = std::make_unique<QBuffer>(backing_mem.get());
        backing_buf->open(QIODevice::ReadOnly);
        animation = std::make_unique<QMovie>(backing_buf.get(), QByteArray());
        animation->start();
        ui->banner->setMovie(animation.get());
#endif
        buffer.clear();
    }
    if (loader.ReadLogo(buffer) == Loader::ResultStatus::Success) {
        QPixmap map;
        map.loadFromData(buffer.data(), static_cast<uint>(buffer.size()));
        ui->logo->setPixmap(map);
    }

    slow_shader_compile_start = false;
    ShowSatronhoTip();
    OnLoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
}

void LoadingScreen::ShowSatronhoTip() {
    if (!Satronho::GuidesEnabled()) {
        tip_label->hide();
        return;
    }
    const Satronho::GuideId id = Satronho::PickPet();
    QStringList tips;
    switch (id) {
    case Satronho::GuideId::Samy:
        tips = {tr("Clic derecho en un juego → «Ficha Satronho»: tiempo jugado, sesiones y respaldo "
                   "de partidas."),
                tr("«Preparar para directo» arma todos los shaders del juego en esta pantalla, "
                   "antes de jugar."),
                tr("«Captura Satronho» (Ctrl+Shift+P) guarda la imagen con nuestro sello, lista "
                   "para tus redes.")};
        break;
    case Satronho::GuideId::Troya:
        tips = {tr("El medidor dice «tirón por shaders» cuando un tirón es de un shader nuevo, no un "
                   "fallo."),
                tr("Satronho → Tema Satronho: Violeta, Noche en negro puro o Ámbar."),
                tr("Al cerrar el juego te cuento cómo te fue: FPS, tirones y si hubo modo directo.")};
        break;
    case Satronho::GuideId::Nono:
        tips = {tr("Con OBS o TikTok LIVE Studio abiertos, el modo directo deja CPU para la "
                   "transmisión."),
                tr("Las configuraciones sugeridas están pensadas para tu gráfica: un clic y listo.")};
        break;
    case Satronho::GuideId::Juana:
        tips = {tr("Cuando sale una build nueva te aviso, y la de Android sale en el mismo release."),
                tr("En Satronho → Novedades ves todo lo que trae cada build.")};
        break;
    default:
        tips = {tr("¿Dudas? Satronho → Pregúntale a los guías: respondemos sin internet.")};
        break;
    }
    const QString tip = tips.at(static_cast<int>(QRandomGenerator::global()->bounded(
        static_cast<quint32>(tips.size()))));
    const Satronho::Guide& guide = Satronho::GetGuide(id);
    tip_label->setText(
        QStringLiteral("<table><tr><td valign=\"middle\"><img src=\"%1\" width=\"44\" "
                       "height=\"44\"></td><td valign=\"middle\" style=\"padding-left:10px\">"
                       "<b style=\"color:%2\">%3:</b> %4</td></tr></table>")
            .arg(Satronho::AvatarPath(id), guide.color.name(), guide.name.toHtmlEscaped(),
                 tip.toHtmlEscaped()));
    tip_label->show();
}

void LoadingScreen::OnLoadComplete() {
    fadeout_animation->start(QPropertyAnimation::KeepWhenStopped);
}

void LoadingScreen::OnLoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value,
                                   std::size_t total) {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    // reset the timer if the stage changes
    if (stage != previous_stage) {
        ui->progress_bar->setStyleSheet(QString::fromUtf8(progressbar_style[stage]));
        // Hide the progress bar during the prepare stage
        if (stage == VideoCore::LoadCallbackStage::Prepare) {
            ui->progress_bar->hide();
        } else {
            ui->progress_bar->show();
        }
        skip_button->setVisible(stage == VideoCore::LoadCallbackStage::Warm);
        skip_button->setEnabled(true);
        previous_stage = stage;
        // reset back to fast shader compiling since the stage changed
        slow_shader_compile_start = false;
    }
    // update the max of the progress bar if the number of shaders change
    if (total != previous_total) {
        ui->progress_bar->setMaximum(static_cast<int>(total));
        previous_total = total;
    }
    // Reset the progress bar ranges if compilation is done
    if (stage == VideoCore::LoadCallbackStage::Complete) {
        ui->progress_bar->setRange(0, 0);
    }

    QString estimate;
    // If there's a drastic slowdown in the rate, then display an estimate
    if (now - previous_time > milliseconds{50} || slow_shader_compile_start) {
        if (!slow_shader_compile_start) {
            slow_shader_start = steady_clock::now();
            slow_shader_compile_start = true;
            slow_shader_first_value = value;
        }
        // only calculate an estimate time after a second has passed since stage change
        const auto diff = duration_cast<milliseconds>(now - slow_shader_start);
        if (diff > seconds{1} && value > slow_shader_first_value) {
            const auto eta_mseconds =
                static_cast<long>(static_cast<double>(total - slow_shader_first_value) /
                                  (value - slow_shader_first_value) * diff.count());
            estimate =
                tr("Estimated Time %1")
                    .arg(QTime(0, 0, 0, 0)
                             .addMSecs(std::max<long>(eta_mseconds - diff.count() + 1000, 1000))
                             .toString(QStringLiteral("mm:ss")));
        }
    }

    // update labels and progress bar
    if (stage == VideoCore::LoadCallbackStage::Build ||
        stage == VideoCore::LoadCallbackStage::Warm) {
        ui->stage->setText(stage_translations[stage].arg(value).arg(total));
    } else {
        ui->stage->setText(stage_translations[stage]);
    }
    ui->value->setText(estimate);
    ui->progress_bar->setValue(static_cast<int>(value));
    previous_time = now;
}

void LoadingScreen::paintEvent(QPaintEvent* event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

void LoadingScreen::Clear() {
#ifndef YUZU_QT_MOVIE_MISSING
    animation.reset();
    backing_buf.reset();
    backing_mem.reset();
#endif
}
