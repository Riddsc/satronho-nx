// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QString>

/// «Captura Satronho»: the game's picture with a discreet band at the bottom (our planet,
/// Satronho NX, the game and its FPS), saved in Pictures/Satronho NX, as on Android.
namespace Satronho::Captura {

/// Where the next capture goes: Pictures/Satronho NX/Satronho_<date>.png.
QString NextPath();

/// Draws the band on the picture.
void Stamp(QImage& picture, const QString& game, double fps);

} // namespace Satronho::Captura
