# Satronho NX: cómo se mantiene y qué hemos aprendido

Satronho NX es el fork de Riddsc de **Lichii** (zortos293/Lichii, emulador experimental de Switch 2,
que a su vez viene de Eglantine → Eden → yuzu/Sudachi). Este archivo es la memoria técnica del
proyecto: léelo antes de tocar algo.

## Repositorio y ramas

- `Riddsc/satronho-nx`, rama **`main`** = Satronho NX. La rama `nx2` es una copia vieja de Lichii; no se usa.
- `main` **no comparte historia git con Lichii**: empezó como snapshot de Lichii `588bfa6`.
  `git merge-base main lichii/nx2` sale vacío. Por eso **no se hace merge**; se aplican diffs.
- Último commit de Lichii integrado: **`bf4c42e2`** (build-117 + arreglos AMD #679, #664, #659).
  Actualiza esta línea cada vez que integres.
- Builds publicadas: release `build-N` en GitHub (N = número de ejecución del workflow).
  build-16 = primer sync a 117; build-17 = duplicado de la 16 (commit re-firmado); build-18 = ajustes
  de niebla/exposición, alineado con Lichii, actualizador; build-19 = detector de esperas lentas.

## Cómo traer cambios de Lichii

```bash
git remote add lichii https://github.com/zortos293/Lichii   # una vez
git fetch --depth=3000 lichii nx2
git log --oneline <ultimo_integrado>..lichii/nx2             # qué hay nuevo
git diff --binary <ultimo_integrado> lichii/nx2 > lichii.patch
git apply -3 --index lichii.patch                            # conflictos → resolver a mano
git diff --stat lichii/nx2   # debe mostrar SOLO los cambios propios de Satronho
```

Al resolver conflictos: **si Lichii ya resolvió lo mismo, gana la versión de Lichii** (no
reprocesar). Mantener solo lo que Lichii no tiene. La última comprobación (`git diff --stat
lichii/nx2`) es la prueba de que no se perdió nada propio ni quedó nada viejo.

## Qué es propio de Satronho NX (todo lo demás es Lichii)

- Marca: nombre "Satronho NX", `satronho-nx.exe` (`src/yuzu/CMakeLists.txt` OUTPUT_NAME), ícono
  (`dist/lichii.ico/png`, 5 guías orbitando un planeta), textos de la UI.
- Actualizador: `CMakeModules/GenerateSCMRev.cmake` apunta a `Riddsc/satronho-nx`;
  `src/common/net/net.cpp` busca `satronho-nx-<plataforma>[-pgo].zip`. El ejecutable auxiliar se
  sigue llamando `lichii-updater.exe` (interno).
- Ajustes de Ocarina: `oot_fog_density` (0–200 %) y `oot_exposure` (25–200 %) en Gráficos;
  100 % = el juego tal cual. Se aplican al traducir dos shaders concretos
  (`src/shader_recompiler/ir_opt/oot_visual_workaround.h`), así que cambian al reiniciar el juego.
- Migración de configuración (`src/frontend_common/config.cpp`, grupo `SatronhoNX`,
  `settings_version`): v1/v2 GPU "Fast", ASTC en GPU, presentación y shaders asíncronos;
  v3 `native_async_draws = Bounded`. Cada versión solo escribe sus propios ajustes.
- Diagnóstico: medidor `EDEN_NX2_GPU_PROFILE` (lectura perezosa del interruptor para que funcione
  desde `nx2_switches.txt`, sitios de Flush/Finish), y detector de esperas lentas
  `vk_slow_wait.cpp` (`EDEN_NX2_SLOW_WAIT_MS`, por defecto 250, 0 = apagado).
- Arreglo del log repetido de umbrales de la caché de texturas (`texture_cache.h`).
- CI (`.github/workflows/release-binaries.yml`): solo Windows, estándar + PGO (clang-cl, ThinLTO,
  perfil de Ocarina de Lichii), se dispara con push a `main`, artefactos `satronho-nx-*`.

## Compilar y entregar

- No se puede compilar ni probar el emulador en el entorno de trabajo remoto: **la compilación de
  GitHub Actions es la primera verificación**. Un push a `main` con cambios en `src/` lanza la build
  (~25 min con caché caliente, ~50 min sin ella).
- Descargar el asset `satronho-nx-windows-x86_64-pgo.zip` de la release con
  `gh api -H "Accept: application/octet-stream" repos/Riddsc/satronho-nx/releases/assets/<id>`.
- Entregar descomprimido en `R:\Claude\APK-APPS\Switch Satronho\Satronho NX build-N` (una carpeta
  por build, junto a las anteriores). Los .exe grandes (satronho-nx.exe, eden-cli.exe) no pasan por
  `stagedPath`: enviarlos con SendUserFile y copiar con su `fileUuid`.

## Datos del usuario y de su PC (para interpretar resultados)

- AMD Radeon **RX 9070 XT** (RDNA4, 16 GB), Ryzen 7 **5800XT**, 64 GB RAM, Windows 11, monitor ~2560×1440.
- Datos en `C:\Users\Ridds\AppData\Roaming\eden` (compartidos con Lichii/Eden): `log\eden_log.txt`
  (sesión actual) y `.old.txt` (anterior), `config\qt-config.ini`, `config\custom\<titleid>.ini`,
  `nx2_switches.txt` del usuario (gana sobre el que viene con la build), caché de shaders en
  `cache\shader\<titleid>\vulkan.fork-v<N>.bin`.
- Juego de prueba: Ocarina of Time `0400486024DEC000`. Configuración recomendada: 1x, FSR o
  ScaleForce (para agrandar 1080p→1440p), SMAA, GPU "Fast", Bounded.

## Cómo leer un log (método que funcionó)

1. Primera línea = versión/build. `LogSettings` = ajustes efectivos de esa sesión.
2. **FPS reales**: líneas `last 600 presents: frame X us (max Y, N over 100 ms) … vblank pacing Z us`.
   Con `vblank pacing` < 1000 µs es juego real (si es ~15000 µs es menú/video a 60 fijos).
   FPS = 1e6 / X. Tirones = N cuadros de más de 100 ms por cada 600.
3. **Shaders**: `Runtime pipelines in the last 10 s: N built, X ms on the GPU thread … Y ms of draws held`.
   X = traducción en el hilo de la GPU (tirones), Y = imagen congelada esperando shaders.
4. **Medidor** (`GPU thread per second` / `GPU back thread per second`, cada 10 s): en Ocarina el
   hilo principal pasa ~2/3 esperando al secundario (`split drain`, `writer wait`) y el secundario
   ~80 % ocupado → el límite es la CPU, no la GPU real. `finishes waiting X ms` y
   `GPU idle waits … from <sitio>` = esperas completas a la GPU real.
5. **Detector**: `Slow GPU wait: X ms … from <sitio>` + lista de pipelines/shaders en vuelo.
6. Comparar siempre a igualdad: misma resolución, misma zona, **caché de shaders caliente**.

## Herramientas de diagnóstico útiles (de Lichii, ver tabla en AGENTS.md)

- `EDEN_NX2_REJECT_HASH=<hash16>,...` en `nx2_switches.txt` del usuario: desactiva esos shaders.
  Sirve para confirmar un culpable (p. ej. el que nombre el detector de esperas lentas).
- `EDEN_NX2_PIPELINE_BUILD_TRACE=<archivo>`: cada build de pipeline del driver con su tiempo.
- `EDEN_NX2_FRAME_DUMP=<dir>`: volcado de un cuadro para fallos visuales.
- Política heredada: los interruptores de experimento no se quedan; un arreglo probado pasa a ser
  comportamiento fijo y uno descartado se elimina. Las herramientas de diagnóstico sí se quedan,
  apagadas por defecto.
- Lichii a su vez integra de Eglantine (`Djihads80/Eglantine`): un arreglo puede estar allí antes.

## Lecciones aprendidas

- **Antes de mantener un parche propio, comprobar si Lichii ya lo resolvió** y de qué forma. Varios
  arreglos propios (NaN en min/max en AMD, reorden de descartes = bigote del Árbol Deku, texturas
  formato 0) quedaron obsoletos porque Lichii los resolvió mejor.
- **Medir antes de quedarse con un "ajuste de rendimiento"**: la espera activa a la GPU y las
  lecturas agrupadas no mostraron ganancia medible y se quitaron.
- **Buscar si una idea ya es una opción de Lichii**: "no congelar al compilar shaders" ya existía
  como `native_async_draws` (Wait/Bounded/Skip); la variable oculta `EDEN_NX2_ASYNC_DISCARD` era peor.
- **Correcciones visuales a ojo = opciones graduables con 100 % = original**, nunca un valor fijo
  por defecto: el usuario prioriza la fidelidad a la consola.
- **La versión de la caché de shaders cambia** cuando Lichii cambia la semántica de los shaders
  (v53→v54 con NaN min/max); la caché vieja se descarta a propósito y la primera sesión tiene
  muchos tirones. No comparar builds con caché fría contra caliente.
- **1x vs 2x solo cambió ~20 % los FPS** en una RX 9070 XT: Ocarina está limitado por los hilos de la
  GPU emulada (CPU). Subir resolución cuesta poco si la GPU sobra.
- **La traducción de shaders no se puede mover a otro hilo** con el diseño actual: lee estado vivo
  de Maxwell (cbufs, TIC, viewport) en el momento del dibujo. Opciones reales: caché caliente,
  precompilación (issue #601 de Lichii).
- **Los "panic" justo después de cerrar el emulador** son el juego reaccionando al cierre forzado:
  no son un fallo.
- Ocarina tiene pausas de 1–3 s al **explotar un Deku Baba**: una espera a la GPU de 1.3–2.6 s
  desde `buffer_cache.h` (DownloadBufferMemory → Finish). También ocurre en Lichii. El detector de
  esperas lentas (build 19) existe para nombrar el shader culpable.
- El usuario no quiere enviar pull requests a Lichii; los aportes se quedan en Satronho NX.
