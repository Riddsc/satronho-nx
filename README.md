# Satronho NX

Satronho NX es una versión propia de Riddsc del emulador **Lichii**, ajustada para tarjetas gráficas AMD.

**Cambios respecto a Lichii (rama `nx2`, 6 de noviembre de 2026):**

- Un descriptor de textura con formato 0 (que no existe en el hardware) se trata como textura vacía en lugar de crear una imagen sobre memoria sin mapear. En The Legend of Zelda: Ocarina of Time esto hacía que los drivers de AMD perdieran la GPU (`VK_ERROR_DEVICE_LOST`).
- Al compilar shaders ya no se leen descriptores fuera de la tabla de texturas (`index > tic_limit`).
- Ajuste de niebla y exposición para Ocarina of Time activado por defecto (viene de la rama experimental `exp/oot-visual-workaround` de Lichii). Solo cambia dos shaders de ese juego. Se apaga con `EDEN_NX2_OOT_VISUAL_WORKAROUND=0` en `nx2_switches.txt`.
- AMD: los mínimos, máximos y clamps de los shaders usan NMin/NMax, que ignoran los NaN como el hardware de Switch (FMin/FMax/FClamp dejaban pasar NaN en AMD: follaje negro, neblina amarilla y agua brillante en Ocarina).
- Aplica una sola vez los ajustes de rendimiento recomendados a configuraciones existentes (ASTC en GPU, precisión Normal, presentación y shaders asíncronos).
- `EDEN_NX2_ASYNC_DISCARD=1` por defecto en `nx2_switches.txt`: los shaders que aún se compilan ya no congelan la imagen.
- Ícono propio de la familia Satronho: el planeta con Samy, Troya, Ñoño, Juana y MechaRidds en órbita (`dist/branding/satronho-nx.svg`).
- Nombre del programa: `satronho-nx.exe`. Solo se compila la versión de Windows (MSVC).

Comparte partidas, configuración y caché de shaders con Lichii, Eglantine y Eden (`%APPDATA%\eden`).

**Créditos y licencia:** basado en [Lichii](https://github.com/zortos293/Lichii) de zortos293, que a su vez viene de Eglantine, Eden, yuzu y Sudachi. Se conservan todos sus avisos de copyright y licencias. Este proyecto se distribuye bajo la misma licencia GPL (ver `LICENSE.txt`). No está afiliado a Nintendo. No incluye juegos, keys ni firmware.

---

<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->

<p align="center">
  <img src="dist/branding/lichii_icon_with_logo.png" alt="Lichii" width="600">
</p>

# Lichii

Lichii is an experimental, open-source Nintendo Switch and Switch 2 emulator written in C++. It builds on Eglantine, Eden, yuzu and Sudachi, preserving their contributions while developing this fork's game compatibility and renderer fixes.

This is an independent fork, not an official release of those upstream projects. Lichii is not affiliated with Nintendo.

Visit the [official Lichii website](https://lichii.zortos.me) to find games and compare community compatibility reports.

## Downloads and setup

Published builds belong on this fork's [releases page](https://github.com/zortos293/Eglantine/releases). The repository still uses its historical Eglantine name; the desktop application is now Lichii.

On Windows, extract the entire build archive and run **lichii.exe**. Only use games and system files you have legally obtained. Games, keys and firmware are not included.

### Existing saves and settings

The rebrand intentionally retains the existing Eden/Eglantine data directories and configuration keys. It does not move or delete saves, settings or shader caches. Back up your saves before testing experimental builds.

## Compatibility and issue reports

Compatibility and performance vary by game, hardware and driver. Rendering errors, stutters and crashes remain possible. Passing automated tests does not establish that a game renders correctly, and this project makes no universal frame-rate guarantee.

Browse [community compatibility reports](https://lichii.zortos.me/reports) for results with each player's build and setup.

Desktop reporting requires a build containing [PR #303](https://github.com/zortos293/Lichii/pull/303) and is not available in build 88. In builds that include it, play and stop a Switch 2 game, then right-click that game and choose **Submit compatibility report**. Only the latest session in the current Lichii process is eligible.

Confirm the matching website catalog entry and review the editable machine, game, setup, and enabled add-on details prefilled where available. Check the public preview and give explicit consent before submitting. Nothing uploads automatically, and the native v1 reporter does not read or upload logs or files.

When [reporting an issue](https://github.com/zortos293/Eglantine/issues), include:

- The exact build, game version, CPU, GPU and driver version.
- Relevant settings and steps to reproduce the problem.
- A screenshot and the log from the affected session, after checking it for personal information.

Do not upload game files, keys, firmware or private account information.

## Building and contributing

See the [build guide](docs/Build.md), [user documentation](docs/user/README.md) and [coding guidelines](docs/Coding.md). Some inherited documentation and internal build target names still use upstream names. The GUI build target remains `yuzu`; its output is `lichii.exe` on Windows and `lichii` on other desktop platforms.

Keep changes focused and distinguish code-level checks from actual gameplay testing.

## Artwork
Art created by YOB (yobslab)
The supplied Lichii logo, icon and library artwork are in [dist/branding](dist/branding). Windows, Linux and macOS icon resources are generated from that artwork by [Generate-LichiiIcons.ps1](tools/Generate-LichiiIcons.ps1).

## Support Lichii

Lichii is free and stays free. If you want to help, you can donate in crypto. Donations help pay for the AI tools and build machines used to develop this open-source project faster.

**Solana (SOL)**
```text
JepEjfyWjGjhJvHSrqj52KtLjcbbbmUh1iGqerYBF4w
```

**Ethereum (ETH)**
```text
0x055ABa1b80Bc35C836Ce7E9F47c2b3d6C75b5534
```

**Bitcoin (BTC)**
```text
bc1qcvxdpf4qvdhqp8ee4s355cjmxrzsphsdwpl8w4
```

**BNB Smart Chain (BNB)**
```text
0x055ABa1b80Bc35C836Ce7E9F47c2b3d6C75b5534
```

**Polygon**
```text
0x055ABa1b80Bc35C836Ce7E9F47c2b3d6C75b5534
```

Send each coin on its matching network. Coins sent on the wrong network can't be recovered.

> [!NOTE]
> Donations are voluntary gifts and are non-refundable. A donation does not buy any product, service, perk, early access, exclusive build, priority support or say in the project. Every release stays free and public for everyone.

## Credits and license

Lichii inherits the work of the Eglantine, Eden, yuzu and Sudachi projects and their contributors, including Eglantine's original developer Camille LaVey. Original copyright notices and third-party licenses are retained.

See [LICENSE.txt](LICENSE.txt) and the source files for applicable license terms.
