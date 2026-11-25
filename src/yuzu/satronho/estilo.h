// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

/// Satronho NX's own themes for the desktop: Violeta, Noche (pure black) and Ámbar, the same three
/// the Android app has. They are palettes over Qt's Fusion style, so every window and dialog gets
/// them without a stylesheet of its own.
namespace Satronho::Estilo {

/// Whether the theme is one of ours ("satronho_violeta", "satronho_noche", "satronho_ambar").
bool IsOurs(const QString& theme);

/// Applies our theme, or puts back the platform's style and palette for any other theme.
/// Returns true when it applied ours (the caller then skips the stylesheet themes).
bool Apply(const QString& theme);

} // namespace Satronho::Estilo
