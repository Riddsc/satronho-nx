# Satronho NX: cómo se mantiene y qué hemos aprendido

Satronho NX es el fork de Riddsc de **Lichii** (zortos293/Lichii, emulador experimental de Switch 2,
que a su vez viene de Eglantine → Eden → yuzu/Sudachi). Este archivo es la memoria técnica del
proyecto: léelo antes de tocar algo.

## Repositorio y ramas

- `Riddsc/satronho-nx`, rama **`main`** = Satronho NX. La rama `nx2` es una copia vieja de Lichii; no se usa.
- `main` **no comparte historia git con Lichii**: empezó como snapshot de Lichii `588bfa6`.
  `git merge-base main lichii/nx2` sale vacío. Por eso **no se hace merge**; se aplican diffs.
- Último commit de Lichii integrado: **`31b7d67e`** (2026-11-12, #1479; build 45 —la 44 falló por una función sin usar—: 52 commits, entre
  ellos #1441 búferes uniformes, #1479 lecturas sin bloqueos, #1449/#1450 cierre, #1485 agua OoT,
  #1493/#1508 generación de cuadros). **Excluido además:** `5b099465` #1365 (tema Lichii Dark,
  ventana What's new, logo de Lichii en Acerca de; forzaba el cambio de tema): se revierte en un
  worktree de lichii/nx2 (`git revert -n`, conflicto en style.qrc → borrar, uisettings.h → dejar
  solo `show_low_ram_warning`) y se aplica el diff 4d884cbb..worktree con
  `--exclude=src/common/recommended_settings.h --exclude='dist/qt_themes/lichii/*'
  --exclude='dist/lichii_wordmark*'`. `IsLichiiTheme()` vive en game_list_p.h (#1371 lo usa;
  siempre falso). Antes:
- Último commit de Lichii integrado antes: **`4d884cbb`** (2026-11-11, #1232; build 38: medidor
  rediseñado #1345, HDR nativo #1326, perfiles DLSS #1249, pipelines con límite de RAM #1334,
  casa de la guardia de OoT #1313, falta de memoria en OoT #1210, chasquido de la ocarina #1196).
  **Excluidos a propósito** (revertidos tras aplicar el diff; vuelve a revertirlos al integrar):
  `290da9e8` #1253 (carpeta de datos propia de Lichii: seguimos en `%APPDATA%\eden`),
  `467e7724` #1200 + `093e264c` #1212 + `17baed71` #1264 (ventana de ajustes recomendados de
  Lichii: tenemos nuestras «Configuraciones sugeridas», y Lichii no nos dio crédito).
  `git show --binary <c> | git apply -R -3 --index`, en orden #1264, #1212, #1200, #1253.
  Antes: `d2eaae4f` (#1143; build 36: arreglo del cierre
  #1058, FSR 3.1/4 para Ocarina #1060-#1062, nxformat 1.0.1 #1078/#1127, reintentos del
  actualizador #1101), `1a5e995a` (#1056, build 35: caché v2 #1036, lectura asíncrona
  #1028, HostTiming #1016, .nx #923, rastreador #975/#1039), `cbb512ee` (#932).
  Actualiza esta línea cada vez que integres.
- Builds publicadas: release `build-N` en GitHub (N = número de ejecución del workflow).
  El historial se reescribió el 2026-11-08 (mismo código, commits a nombre de Riddsc) y las builds
  2–20 se retiraron. build-21 = primera del historial reescrito: Lichii hasta bf4c42e, ajustes de
  niebla/exposición, alineado con Lichii, actualizador propio y detector de esperas lentas.
- Commits: autor Riddsc <richardsc921@gmail.com>, sin firmas ni menciones de IA en mensajes,
  archivos ni releases.

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
  `src/common/net/net.cpp` busca `satronho-nx-<plataforma>[-pgo].zip`. El ejecutable auxiliar
  es `satronho-updater.exe` desde la build 26 (antes `lichii-updater.exe`; una instalación vieja
  usa su `lichii-updater.exe` para instalar la 26, que trae ya el nuevo; el viejo queda sobrante).
- Marca completa (build 27): textos visibles con "Lichii", "Eden" o "Eglantine" pasan a "Satronho NX"
  (firmware, migración, accesos directos, multijugador, actualizador, atajo "Exit Satronho NX" — el
  atajo personalizado viejo "Exit Eglantine" se pierde y vuelve al de fábrica), CLI
  `satronho-cli.exe` (`src/CMakeLists.txt` OUTPUT_NAME; antes `eden-cli.exe`), User-Agent
  `Satronho-NX`, enlaces de "Acerca de" a Riddsc/satronho-nx con crédito a Lichii. Los informes
  de compatibilidad siguen yendo al sitio de Lichii (lichii.zortos.me): el usuario quiere
  compartirlos; ya se identifican como "Satronho NX, fork of Lichii build-N (commit)".
  Se mantienen a propósito: el texto de "Acerca de" que dice que se basa en Lichii, Eglantine y Eden
  (crédito/licencia GPL), los avisos de copyright, la carpeta de datos `AppData\Roaming\eden`
  (compartida con Lichii: cambiarla perdería configuración y cachés), "cuenta de Eden" en
  multijugador (los servidores de Eden; al usuario le parece bien), los nombres de otros emuladores en la migración, y el ID
  de Discord (es la aplicación de Eden; para mostrar "Satronho NX" hace falta crear una propia en
  el portal de desarrolladores de Discord). Al integrar Lichii, estos textos pueden dar conflictos:
  conservar los de Satronho.
- Ajustes de Ocarina: `oot_fog_density` (0–200 %) y `oot_exposure` (25–200 %) en Gráficos;
  100 % = el juego tal cual. Se aplican al traducir dos shaders concretos
  (`src/shader_recompiler/ir_opt/oot_visual_workaround.h`), así que cambian al reiniciar el juego.
- Migración de configuración (`src/frontend_common/config.cpp`, grupo `SatronhoNX`,
  `settings_version`): v1/v2 GPU "Fast", ASTC en GPU, presentación y shaders asíncronos;
  v3 `native_async_draws = Bounded`. Cada versión solo escribe sus propios ajustes.
- Diagnóstico: medidor `EDEN_NX2_GPU_PROFILE` (lectura perezosa del interruptor para que funcione
  desde `nx2_switches.txt`, sitios de Flush/Finish), y detector de esperas lentas
  `vk_slow_wait.cpp` (`EDEN_NX2_SLOW_WAIT_MS`, por defecto 250, 0 = apagado). Desde la build 22
  pone timestamps de GPU a cada despacho de cómputo y, en cada espera lenta, escribe una segunda
  línea `Slow GPU wait for tick N: X dispatches took Y ms on the GPU, slowest first` con el tiempo
  real de cada shader. `Scheduler::Wait` también anota su sitio (antes salía `(unknown)`).
  Desde la build 24 la lista muestra todas las etapas de cada pipeline gráfico con su nombre
  (`vsA vs tcs tes gs fs`), y con `EDEN_NX2_SLOW_WAIT_DRAWS=1` también mide en la GPU cada dibujo
  gráfico (reinicia las consultas desde el host: necesita hostQueryReset; usa 131072 ranuras).
  Cuesta algo de FPS, por eso va apagado por defecto.
  También desde la build 24, cada espera lenta añade una tercera línea con los envíos (submissions)
  de esos ticks: cuándo llegó cada uno a la cola respecto al inicio de la espera (`submitted`), el
  tiempo que la GPU estuvo parada antes de empezarlo (`idle`: un semáforo o un envío tardío), y la
  duración en la GPU de sus subidas (`uploads`, el buffer que va primero: copias, decodificación de
  ASTC) y de sus comandos (`commands`).
  Desde la build 30, una cuarta línea dice qué tuvo ocupado al hilo `VulkanWorker` durante la
  espera: cada compilación de pipeline de cómputo que tuvo que esperar (hash y ms; además se
  avisa en el log al pasar de 100 ms) y cada bloque de comandos de más de 100 ms.
- (El arreglo propio del log repetido de umbrales de la caché de texturas se reemplazó por el de
  Lichii en la build 35.)
- Diagnóstico en conjunto (build 35): `StutterTrace::SetFrameDetailHook` (añadido a
  `nx2_stutter_trace.{h,cpp}`) agrega `waited=` a cada línea `Nx2StutterDetail` con lo que el
  VulkanWorker esperó en ese cuadro (`SlowWait::FrameWorkerText`: `compute:<hash>:<ms>`,
  `gfx:<clave>:<ms>`, `chunk:<ms>`). Con `EDEN_NX2_STUTTER_TRACE=1` se guardan esperas desde 4 ms
  (trozos desde 20 ms) en un anillo de 512; las esperas lentas siguen listando solo las de 100 ms o
  más. Las esperas por pipelines gráficos (`vk_graphics_pipeline.cpp`) también se anotan.
  Conflicto típico al integrar: el lambda de espera de `vk_compute_pipeline.cpp` (conservar la
  zona `PipelineWait` y `jobs->RunOrWait` de Lichii más nuestro `wait_start`/`NoteWorkerBuild`) y
  `vk_master_semaphore.cpp` (zona `GpuWait` + nuestro cronómetro).
- Medidor Satronho (build 40; la ejecución 39 quedó cancelada porque le faltaba meter.cpp en CMakeLists, `src/yuzu/satronho/meter.{h,cpp}`): medidor propio que reemplaza al
  `PerformanceOverlay` de Lichii (sus archivos quedan intactos y sin usar, para integrar sin
  conflictos; `perf_overlay` en `main_window` es `Satronho::Meter*`). Mismos datos que el de
  Lichii (PerfStats, DkbCpuTrace, GPU busy) más: tirones >100 ms y por minuto, shaders armándose
  (`MainWindow::ShadersBuildingNow`), modo Docked/Handheld y Fast/Accurate, RAM
  (`Common::GetProcessMemoryUsage`) y VRAM (`RendererBase::GetVideoMemoryUsage`: uso + holgura de
  VMA contra el presupuesto; naranja 85 %, rojo 97 %). Opciones en `UISettings::satronho_meter_*`
  (filas por bits `Meter::Row`, tamaño, fondo, color, esquina/libre x-y, compacto), editables con
  clic derecho o «Satronho → Medidor». El texto del menú View dice «Show Satronho Meter» (main.ui).
  Tema de Samy: `medidor`. Vista previa sin emulador: harness con stubs (`meterharness` en el
  scratchpad, compila meter.cpp con Qt de PySide).
- Conflicto en `vk_rasterizer.cpp` (build 38): las lambdas de despacho llevan
  `slot = note_slow_wait()` + `SlowWait::ResetSlot` nuestro y `RecordDispatchBarrier` de Lichii.
- Caché v2: los registros guardan el código nativo, no SPIR-V, así que el ajuste de niebla y
  exposición de Ocarina (`CreateGraphicsPipeline`/`CreateComputePipeline`) se sigue aplicando.
- CI (`.github/workflows/release-binaries.yml`): solo Windows, estándar + PGO (clang-cl, ThinLTO,
  perfil de Ocarina de Lichii), se dispara con push a `main`, artefactos `satronho-nx-*`.

## Compilar y entregar

- No se puede compilar ni probar el emulador en el entorno de trabajo remoto: **la compilación de
  GitHub Actions es la primera verificación**. Un push a `main` con cambios en `src/` lanza la build
  (~25 min con caché caliente, ~50 min sin ella).
- Descargar el asset `satronho-nx-windows-x86_64-pgo.zip` de la release con
  `gh api -H "Accept: application/octet-stream" repos/Riddsc/satronho-nx/releases/assets/<id>`.
- Entregar descomprimido en `R:\Claude\APK-APPS\Switch Satronho\Satronho NX build-N` (una carpeta
  por build, junto a las anteriores), y dentro de esa carpeta también el zip tal cual
  (`Satronho NX build-N (PGO).zip`) para que el usuario lo comparta sin bajarlo de GitHub. Los .exe grandes (satronho-nx.exe, eden-cli.exe) no pasan por
  `stagedPath`: enviarlos con SendUserFile y copiar con su `fileUuid`.

## Datos del usuario y de su PC (para interpretar resultados)

- AMD Radeon **RX 9070 XT** (RDNA4, 16 GB), Ryzen 7 **5800XT**, 64 GB RAM, Windows 11, monitor ~2560×1440.
- Datos en `C:\Users\Ridds\AppData\Roaming\eden` (compartidos con Lichii/Eden): `log\eden_log.txt`
  (sesión actual) y `.old.txt` (anterior), `config\qt-config.ini`, `config\custom\<titleid>.ini`,
  `nx2_switches.txt` del usuario (gana sobre el que viene con la build), caché de shaders en
  `cache\shader\<titleid>\vulkan.fork-v<N>.bin`.
- Juego de prueba: Ocarina of Time `0400486024DEC000`. Configuración recomendada: 1x, FSR o
  ScaleForce, SMAA, GPU "Fast", Bounded.
- Riddsc juega en **modo Docked** al menos desde la build 30 (`System.use_docked_mode: Docked`, con
  `M-` por ser distinto del valor por defecto, Handheld). En Docked Ocarina compone a 2560×1440
  (`nx2:vi2 composite image … layout 2560x1440`), justo su monitor; en Handheld sería 1920×1080.
  Antes de comparar sesiones, revisar que ambas tengan el mismo modo.

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
5. **Detector**: `Slow GPU wait: X ms … from <sitio>` + lista de pipelines/shaders en vuelo, y
   `Slow GPU wait for tick N: …` con el tiempo de GPU de cada shader de cómputo. Si la suma de
   cómputo no explica la espera, el culpable es un shader gráfico.
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
  esperas lentas (build 19) existe para nombrar el shader culpable. En la sesión de la build 19
  (65 min, varias zonas) 12 de 13 pausas duraron casi lo mismo (1.40–1.51 s), casi siempre al
  pelear las primeras veces con un enemigo: parece un límite fijo (los bucles de shader se cortan
  a 0x2000 vueltas, `emit_spirv.cpp`), sin confirmar. Sospechoso: compute `e2ce39e630ce65f3`
  (32x32, presente en las 12). La build 22 mide cada despacho para confirmarlo.
  **Resultado build-23 (2026-11-08):** 3 pausas de 1412/1436/1471 ms; todo el cómputo de cada una
  sumó < 1 ms de GPU, así que `e2ce39e630ce65f3` queda descartado (0.0 ms). El único gráfico
  presente en las tres es `vs a2c7af4889370200` (nuevo sospechoso). Ojo: el detector imprime como
  "fs" el hash del índice 4, que es geometry; el fragment es el índice 5 (orden VertexA, VertexB,
  TessCtrl, TessEval, Geometry, Fragment). Corregido en la build 24, que además puede medir los
  dibujos gráficos (`EDEN_NX2_SLOW_WAIT_DRAWS=1`).
  **Resultado build-23, segunda sesión (peahats, con `REJECT_HASH=a2c7af4889370200`):** 15 pausas
  de 1.42–1.86 s, casi siempre en pareja (`buffer_cache.h:404` y luego `:2801`, ~3 s por evento);
  el rechazo no actuó ni una vez (0 líneas `rejected by`), y en varias pausas ese shader ni
  aparece. Clave: esperas de ~1.5 s por un tick con solo 3 despachos que suman 0.1 ms de GPU y
  ningún dibujo. El tiempo no está en los shaders: o el envío llegó tarde, o la GPU estuvo parada
  (semáforo), o está en el buffer de subidas (copias/ASTC), que no se medía. Sin relación clara con
  las compilaciones de pipelines.
  **Build-25 (Lichii hasta bbf3e629), misma pelea desde el mismo guardado: 0 esperas lentas**, peor
  cuadro 135 ms, mediana 34 FPS (sesión corta, ~2 min de juego). Las pausas de 1.5 s parecen
  resueltas por algún commit de Lichii posterior a bf4c42e2; candidatos: #752 (un canal espera una
  lista de comandos escrita por la GPU sin bloquear a los demás), #337 (consultas con fence
  pendientes hasta que corre su fence), #621. Sin confirmar cuál; vigilar en sesiones largas.
  **Build-28 (20 min de juego, mediana 43 FPS): 2 esperas seguidas de 1381/1388 ms.** La línea de
  envíos lo explicó: el tick llegó a la cola **1379 ms después** de empezar la espera y la GPU
  estuvo parada ese tiempo (`idle 1394.9 ms`); sus comandos duraron 1.3 ms. O sea, el hilo
  `VulkanWorker` tardó 1.4 s en llegar a enviar. Causa probable: `ComputePipeline::Configure`
  graba en el worker una espera a que el pipeline de cómputo termine de compilarse (`RunBuild` +
  `build_condvar.wait`), y el driver de AMD tarda ~1.4 s con ciertos shaders de cómputo nuevos.
  Encaja con "la primera vez que peleo con un enemigo" y con que la 25 no pausara en una pelea ya
  vista (el driver ya tenía ese pipeline en su caché). Confirmar con
  `EDEN_NX2_PIPELINE_BUILD_TRACE=<archivo>` (líneas END compute con ms≈1400 a la hora de la
  pausa). Arreglo ya hecho por Lichii (#781, #812): clic derecho en el juego → "Build Shader
  Precache" y "Precache shaders before game start" (Graphics → Advanced) en Ask o Always.
  **Confirmado en la build 30:** las dos pausas de la sesión (2555 y 2021 ms) coincidieron al
  milisegundo con `Compute pipeline a2a69408ebf4ba53 / 34c23e5edd42e3c5 kept the Vulkan worker
  waiting …`. El rastro de compilaciones mostró ~125 shaders de cómputo que en sesiones anteriores
  salían de la caché (0 ms) y en la primera sesión de la build 30 se compilaron otra vez (2–5 s
  cada uno): tras una build que cambia la traducción de shaders, el driver no los reconoce. Por eso
  la primera sesión después de actualizar tiene pausas y texturas tardías; el precache las evita.
  **Sesión de 67 min con la build 30 (2026-11-08):** solo 3 pausas (2369, 2243, 2330 ms), cada
  una un shader de cómputo nuevo (5dbb633ac79d913f, fd6880439158691e, ade9ffd111cf30a1) que nunca
  se había visto; los de las pausas anteriores (a2a69408…, 34c23e5e…) ya salieron de la caché del
  driver en 0.6 y 0.2 ms. Sin cierres (el PANIC del final es del cierre forzado al salir). Idea para
  quitar también esas: compilar primero sin optimizar (`VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT`,
  ya usado en `pipeline_optimization.h`) y cambiar a la versión optimizada cuando termine.
- **Cementerio de Kakariko (build-23, video de un amigo, de noche con lluvia)**: paredes y lápidas
  como bloques oscuros con rayas verticales, sobre todo cerca de la cámara; en otra zona, paredes
  muy verdes. Sin diagnosticar. Relacionados en Lichii: #760 (luz del cementerio), #441 (AMD,
  basura en piedras por pesos a cero en la composición) y #820 (varyings noperspective recortados
  junto a la cámara en AMD; viene en la build 24). Probar primero en la build 24 en el mismo punto.
- **Al escribir un archivo en el PC con `device_commit_files` usar un `stagedPath` con nombre
  nuevo cada vez**: reutilizar la misma ruta envió la versión anterior en caché (el
  `nx2_switches.txt` quedó con el contenido viejo aunque la herramienta dijo "written"). Releer
  el archivo del PC después de escribirlo.
- **Cierre al cargar la partida (builds 25 y 28, RX 6700 XT de un amigo; la 23 no):** volcados de
  Windows con `abort()` en `ucrtbase.dll+0xa527e` (fast-fail 7) al entrar al juego; uno guardó en
  memoria `surface.h:193:BitsPerBlock: unreachable`. Era una regresión de Lichii #858 (memo de
  texturas preparaba la imagen nula, de formato inválido): Lichii #904, arreglado en #910, #921 y
  #918 (Log2Ceil(0)). La build 29 se canceló; sale en la build 30. También se cerraba así en la PC
  de Riddsc al entrar a Kakariko (dos veces con la build 28). Lección: leer volcados con el paquete Python
  `minidump` y buscar en la memoria cruda las últimas líneas del log (`[ t] Clase <Critical> …`).
- **Guías (build 32):** guías (MechaRidds, Samy, Troya, Ñoño, Juana), Novedades,
  Configuraciones sugeridas con botón automático y base de conocimiento local (código en
  `src/yuzu/satronho/`, datos en `dist/satronho/`). Sale en la build 32 (la 31 falló al compilar por una función sin usar: el CI usa -Werror=unused; revisar en local con esas mismas opciones). La base de conocimiento
  se probó con una réplica en Python del buscador (192 preguntas).
  **Build 33:** burbuja de los guías en la esquina inferior derecha de la lista de juegos
  (`dock.cpp`, hija de `game_list`, así que se oculta sola mientras se juega; máscara para no
  tapar clics; se apaga con clic derecho o en Guías, ajuste `satronho_bubble`). Da consejos de la
  base de conocimiento y Juana anuncia ahí la build nueva (si la burbuja está apagada, se abre la
  ventana de Novedades como antes). Arreglado el recuadro de guías encimado arriba a la izquierda
  en Configuraciones sugeridas: `deleteLater` dentro de `exec()` no borraba el banner viejo hasta
  cerrar la ventana; ahora se borra en el momento. Las novedades se redactan destacando nuestro
  trabajo (integramos, probamos, rastreamos), sin atribuirnos arreglos ajenos.
  Revisión visual local: arnés offscreen con Qt 6.8.3 de la rueda de PySide6 (trae `uic` y `rcc`,
  no `moc`); con `uic` también se puede revisar `main_window.cpp`.
  **Error de la build 32-33, corregido en la 34:** Configuraciones sugeridas recomendaba Wait y
  VRAM Aggressive (≥15 GiB). Sesión de Riddsc con la 33 tras «Aplícalo por mí» (y en stream): 100
  esperas lentas (total 103 s, máx. 14.5 s, 26 de más de 1 s) contra 3 con la 30 en Bounded; el
  Vulkan worker se quedaba ejecutando un trozo de comandos 1–3 s esperando pipelines gráficos
  nuevos (Wait). Se vuelve a Bounded (también por migración de ajustes v4) y Conservative hasta
  16 GB. ASTC no cambia nada aquí: bajo 20 GB Satronho usa BC3 igual (`UseBc3ForSwitch2OnSmallCards`).
  Regla: no recomendar un ajuste que contradiga lo que ya se eligió y probó antes (v3 = Bounded).
  **Confirmado con la build 34 (2026-11-09, 67 min):** Bounded restablecido por la migración v4,
  0 esperas lentas, mediana de 16.9 ms por cuadro (59 FPS; p90 21.4 ms) y 71 cuadros de más de
  100 ms, contra 351 con la 30 y 848 con la 33. VRAM seguía en Aggressive (la migración no la
  toca) sin problemas visibles.
  Configuraciones sugeridas cubre AMD (RX 9000/7000/6000, anteriores e
  integradas), NVIDIA (RTX 50/40/30/20, GTX 16/10, profesionales) e Intel (Arc A/B, Arc
  integrada, UHD/Iris Xe); el clasificador se probó con 31 nombres reales de Vulkan.
- **GitHub a veces responde 502/504 al bajar herramientas** (LLVM, sccache, dependencias CPM):
  desde la build 22 la compilación reintenta cada descarga y la configuración de CMake.
- No se hacen pull requests a Lichii; los cambios se quedan en Satronho NX. Dar crédito a Lichii y
  compartir informes de compatibilidad con ellos sí está bien (es un fork), siempre que quede
  claro que viene de Satronho NX. La carpeta `AppData\Roaming\eden` se queda así a propósito
  (trazabilidad de partidas y datos anteriores).
- **Líneas negras en cuadrícula sobre el campo de Hyrule** (se ven de lejos y desaparecen al
  acercarse; issues abiertos de Lichii #603, #709, #675, todos en AMD): **ya corregido por Lichii** en
  `bf4c42e2` (#679, "#448 uneven BCn mips stay RGBA8 on AMD"), posterior a su build 117 y ya integrado
  aquí. Causa: ASTC recomprimido a BC1/BC3 con mips de tamaño en bloques desigual; el driver AMD los
  muestrea mal en las costuras del terreno lejano, así que esas texturas quedan en RGBA8. Solo actúa
  con ASTC decodificado en GPU y recompresión distinta de "Uncompressed". Verificado por el usuario
  en build-23 (RX 9070 XT) en el mismo punto donde Lichii 117 mostraba las líneas.

## Build 36: método de medición y caché de cómputo en AMD

- **Recorrido oficial de prueba:** Docked, Fast, desde la llanura de Hyrule hasta el río Zora, unos
  5–10 minutos. Cada build se juzga ahí, con `tools/satronho/compare_logs.py viejo.txt nuevo.txt`
  (misma tabla siempre y avisos si las sesiones no son comparables: modo, precisión, filtro,
  primera sesión tras cambiar la caché, rastreador, duración).
- **Prueba 34 contra 35 en ese recorrido (2026-11-09):** la 35 no era más lenta. La 34 dio 36.9 FPS
  típicos con 12 congeladas de 1.5–5.5 s; la 35, 48 FPS sin congeladas. Las congeladas de la 34
  fueron los mismos 12 shaders de cómputo que la 35 había recompilado: el historial de
  `pipeline_builds.txt` muestra que 74ab167d83c54073 (y otros) tardaban 4–6 s cada pocas sesiones
  desde antes de la 35, con 0.2–0.8 ms entre medio: **el driver propietario de AMD los saca de su
  propia caché.** Lichii no usa caché de pipelines propia en AMD/NVIDIA
  (`Device::DriverCachesPipelines`) y en AMD tampoco VK_KHR_pipeline_binary.
- **Arreglo (build 36):** en AMD propietario, los pipelines de cómputo se construyen en una
  `VkPipelineCache` nuestra (`satronho_compute_pipelines.fork-v54.bin` en la carpeta de shaders del
  juego), cargada en `LoadDiskResources`, guardada al terminar la carga, tras el precache, a los
  20 s de cada pipeline de cómputo nuevo y al cerrar; se borra con «Remove Vulkan Driver Pipeline
  Cache». Para verificarlo: tras una sesión, en la siguiente no debe aparecer «kept the Vulkan
  worker waiting» para hashes ya vistos, y el log dice «Satronho compute driver cache: N bytes».
- **Sesiones con la 36 (2026-11-09/10), con el comparador corregido** (sin los tramos quietos a 60
  FPS exactos de menú/pausa/título, que antes inflaban los FPS: la 1.ª sesión tenía 9.8 min así):
  | | 34 | 35 | 36 (1.ª) | 36 (2.ª) |
  | FPS típicos al jugar | 36.6 | 42.2 | 38.8 | 38.2 |
  | 10 % más lento | 30.2 | 31.2 | 33.2 | 26.5 |
  | Cuadros >100 ms/min | 16.8 | 14.2 | 8.3 | 15.5 |
  | Congeladas por shaders | 12 (hasta 5.5 s) | 0 | 0 | 0 |
  La caché de cómputo de AMD guardó 22.6 MB y los recargó en las sesiones siguientes; ningún shader
  frenó al worker. Los FPS no cambiaron entre builds: los tirones que quedan son de categoría
  «other» (lado de la emulación/CPU), no shaders. FSR 4 (sesión corta): 4.1.2, sale a 3840×2160
  (supermuestreo en monitor 1440p), ~6 s de congelada al iniciarse y ~37 FPS contra ~48; no se
  recomienda en Configuraciones sugeridas por ahora.
- **Build 37 en stream (2026-11-10, 74 min, 28 jugando):** 31.9 FPS típicos (el stream le quita
  CPU: sin stream ~38–39), 12.8 cuadros >100 ms/min. 2 congeladas de ~2.3 s: a2ad0e377bf35f6f y
  ac898ad3f17a4df5, shaders de cómputo vistos una sola vez antes (5.8–6.3 s, en la build 30, antes de
  existir nuestra caché) en zonas que no se habían vuelto a visitar; ahora quedan en ella. Ninguno de
  los que ya estaban en nuestra caché volvió a compilarse.
- **Builds 38 y 40 (2026-11-11), Docked/Fast/FSR/SMAA, Bounded, Conservative:**
  | | 38 (1.ª tras actualizar, 5.5 min) | 40 (24 min jugando, 60 en total) |
  | FPS típicos al jugar | 43.4 | 44.3 |
  | 10 % más lento | 33.8 | 41.3 |
  | Cuadros >100 ms/min | 8.0 | 3.4 |
  | Congeladas por shaders | 1 (1.7 s) | 0 |
  La 38 re-tradujo 4210 programas («of another build lowered again»: los commits de Lichii tocaron
  el compilador de shaders), así que su SPIR-V cambió y el driver no lo tenía: el cómputo
  37fa0bf79db426d7 (5.4 s en una build vieja) tardó 1.66 s; en la 40 salió de nuestra caché en
  1.1 ms. Tras una actualización que toca el compilador, la primera sesión vuelve a armar shaders.
  FPS por encima de la 36 (~38–39) en ambas; la sesión de la 40 no fue solo el recorrido oficial.
  La 40 forzó la detención al cerrar («Emulation thread failed to stop»), la 38 no.
- **Hookshot (port nativo de OoT, NotRexed/hookshot, GPL-3.0, revisado 2026-11-11):** recompilación
  estática (ARM64 → C → clang) + Horizon y NVN2 reimplementados; reproduce las llamadas NVN2 en
  Vulkan con el recompilador de shaders de Eden/Eglantine (su FSR 4 viene de la rama OoT de Lichii).
  Nada de su núcleo se puede trasplantar a un emulador. Su perfil de CPU (`docs/cpu-profile.md`,
  RX 9070 XT + Ryzen 7 5700X3D, 64 GB): llanura de Hyrule 51 FPS con 8 cuadros >100 ms, Kokiri 77,
  ciudadela→Templo del Tiempo 91; el código del juego solo ya cuesta 1.7–2.1 núcleos. Nuestra 40
  en llanura→Zora: 44 FPS típicos (~86 % del nativo ahí). 60 FPS fijos en la llanura no los da ni el
  port en ese equipo. Idea útil: su método de medir qué limita el cuadro (tiempo por hilo y en qué
  espera); nosotros no registramos aún cuánto trabajan los núcleos del juego.
- **Hookshot en el PC de Riddsc (2026-11-11, v2.1.0, 2560×1440, tope 144, sin generación de cuadros,
  recorrido largo que incluye llanura→Zora):** 9.5 min de juego en ventanas de 5 s: mediana 50.3
  FPS, 10 % más lento 40.3, mínimo 32.3; solo el 20 % del tiempo a 60 o más. GPU ~10 ms por cuadro
  (p90 14): también limitado por CPU. Satronho 40 (ventanas de 10 s, sesión distinta): 44.3 / 41.3,
  o sea ~88 % de la mediana del port. Techo práctico en la llanura en este PC: ~50 FPS.
- **Build 41: medición de núcleos.** `LogCores` en `nx2_stutter_trace.cpp` (con cada
  Nx2StutterSummary): «Satronho cores window_s guest=0:x,... busiest_guest guest_sum gpu_command
  gpu_back vulkan_worker», % de un núcleo del host (DkbCpuTrace). compare_logs.py lo muestra
  (medianas en ventanas de juego). El medidor tiene la fila `Cores` (barras; naranja ≥95 %); las
  filas guardadas en 0x3FF (default de la 40) pasan a 0x7FF. Tema de Troya: `techo-fps`.
- **Sesión de la 41 (2026-11-12, 51 min jugando, recorrido largo con zonas nuevas):** 43.3 FPS
  típicos, 31.7 el 10 % más lento, 3.3 tirones/min, 42 shaders >100 ms (máx. 5.8 s) y 100 esperas
  largas. Los 42 son cómputos que no se armaban desde antes de la build 36 (líneas ~75–80k de
  pipeline_builds.txt), o sea antes de existir nuestra caché de cómputo; 1738 programas sin
  registro en la caché v2. Costo de una sola vez: la caché pasó de 23 a 30 MB.
  Núcleos (400 ventanas de juego): los 6 núcleos del juego entre 62 y 72 % de mediana, ninguno por
  encima de 92 %; suma ~409 % (el port nativo gasta 167–214 % en el código del juego). GPU back 63 %,
  GPU command 48 %, worker de Vulkan 24 %. En ventanas <38 FPS los núcleos trabajan MENOS
  (57–67 %) que en ventanas ≥44 FPS (67–86 %): no hay un hilo saturado, los hilos esperan. Siguiente
  medición: en qué esperan los núcleos del juego (sincronización con la GPU emulada, locks, etc.).
- **Build 42: esperas.** `src/common/satronho_waits.{h,cpp}`: esperas de los hilos del juego <100 ms
  por tipo (gpu_read = GPU::OnCPURead, gpu_sync = evento NVCTRL::NvEvent, present = BufferQueue/
  VI::VsyncEvent, threads = condvar/address/eventos del juego/timeout, sleep, services = IPC por
  nombre de servicio: `NamePort` en sm.cpp y server_manager.cpp). Ganchos en svc_synchronization,
  svc_condition_variable, svc_address_arbiter, svc_thread (SleepThread), svc_ipc y gpu.cpp.
  `LogWaits` escribe «Satronho waits gpu_read=ms/n ... top=etiqueta=ms/n,...» con cada resumen;
  compare_logs.py tiene filas «ms por 10 s esperando …». El medidor muestra «Esperan sobre todo a …»
  bajo las barras. `Service::Event(ctx, name)` nuevo para nombrar el evento de vsync.
- **Sesión de la 42 (2026-11-12, 40 min jugando; recorrido oficial al final, t≈4290–4903 s):**
  oficial: 44.2 FPS típicos, 35.0 el 10 % más lento, 3.8 tirones/min, 0 shaders; núcleo más ocupado
  64 %, suma 345 %, GPU back 78 %. Esperas cortas por 10 s en el oficial: threads 216 s (21 mil
  esperas/s, ~1.0 ms c/u, casi todo `threads:address` = trabajadores del juego sondeando),
  sleep 16.7 s (~1.2 ms c/u), services 13.5 s (IAudioRendererEvent ~9.7 s del hilo de audio;
  `services:?` ~860 llamadas/s a sub-interfaces sin nombre, ~0.9 ms c/u), gpu_read 1.2 s
  (~94/s, 1.25 ms c/u), gpu_sync y present 0 (los eventos se llaman `nx2:syncpt:Slot0`,
  `nx2:gsv:SyncptNotify`, `nx2:gpu:Event1`: caen en services; corregir la clasificación).
  Lo de ~1 ms por espera es la granularidad del reloj (HostTiming duerme en el SO, Lichii #1016):
  Lichii midió que la espera por hardware NO cambia FPS en OoT (p50 0.1–0.3 ms tarde) → no es
  objetivo. Candidatos reales: lecturas de GPU (~12 % de un hilo) y las ~860 llamadas/s a servicios
  a ~1 ms cada una (falta saber cuáles y si el ms es cola del hilo del servicio o trabajo).
  La línea del medidor dice casi siempre «sus otros hilos» por los trabajadores sondeando: excluir
  `threads:address` (y los timeouts) de esa línea.
- **Vuelta oficial limpia con la 42 (recién abierto, 4.7 min):** 45.4 FPS típicos, 32.3 el 10 % más
  lento, 8.6 tirones/min, 0 shaders. Ruido entre vueltas: FPS típicos ±1; el 10 % más lento ±3; los
  tirones dependen del calentamiento (8.6/min recién abierto contra 3.8 tras una hora). Para A/B:
  vuelta oficial recién abierto en ambas builds y comparar sobre todo los FPS típicos.
  Esperas de GPU en esa vuelta: lecturas ~130–230 ms/s («readers waited on their fence» hasta 227
  ms/s, la más larga 10 ms) y syncpoints nx2:syncpt:Slot0 ~120 ms/s; GPU back 77 %. Es lo más grande
  que no es ocioso. Candidato a la primera mejora.
- **Cierre colgado (40, 42 y 42b; «Emulation thread failed to stop»):** tras la petición de cierre,
  el desmontaje sigue 7–9 s después de `~Cache` (también en los cierres buenos: 38 tardó 9.4 s y 41
  10.8 s desde OnShutdownBegin). Lichii termina el hilo a los 10 s del ForceStop
  (`EmuThreadStopTimeoutMs`) y, terminado a mitad del desmontaje, la ventana se cuelga. Build 43:
  30 s y `ShutdownMainProcess` registra cada paso de >100 ms («Satronho shutdown: X took N ms»).
- **Build 43:** además, nuevo tipo `polling` (arbitraje de direcciones, timeouts, eventos de audio:
  no frenan), los eventos `nx2:syncpt/gsv/gpu` cuentan como gpu_sync, las lecturas de GPU llevan el
  hilo (`t<id> p<prioridad>`) y las fases `phase:gpu_thread` / `phase:download`. Tema de Juana:
  `cierre-colgado`.
- **Sesión de la 43 (recién abierto, 6.3 min jugando):** cerró solo en 11.1 s desde OnShutdownBegin:
  `kernel.CloseServices()` 1.3 s, `gpu_core.reset()` **8.2 s** (el desmontaje de la GPU/Vulkan es lo
  lento), `kernel.Shutdown()` 0.5 s. 38.5 FPS típicos / 34.1 el 10 % más lento / 5.8 tirones/min,
  pero la vuelta pasó ~4 min seguidos en un tramo pesado a ~36 FPS (la 42b tuvo tramos iguales de
  35–39): los FPS típicos dependen de cuánto se queda uno en los tramos pesados; corregido el «±1».
  Comparar por tramos (pesado <40 / liviano ≥44). En ventanas <40 FPS: esperas a syncpoints de la GPU
  (nx2:syncpt:Slot0) 326 ms/s contra 67 ms/s en ventanas ≥44; GPU back 84 % contra 72 %; núcleos del
  juego 47–55 % contra 57–66 %. Conclusión: en los tramos pesados el límite es el hilo «back» de la
  GPU emulada. Su perfil (Nx2Profile, 10 s): host_cpu_busy ~850 ms/s; method 220 ms/s (4.4 M
  métodos/s, el salto de escrituras repetidas ya está: EDEN_NX2_SKIP_SAME), split wait 156 (ocioso),
  bind stage buffers 105 (178 mil/s), other 93, draw 64, update buffers 55, configure 42, compute
  36–61, read handles 35. ~87 mil dibujos/s. Lecturas de GPU: ~80 ms/s, mitad esperando al hilo de
  GPU y mitad la descarga; el hilo lector cambia de id entre zonas (p44).
- **Sesión de la 45 (primera tras integrar 52 commits de Lichii):** el recompilador cambió
  (#1468, #1511): «6523 programs of another build lowered again», 3673 sin registro durante el juego,
  la caché de cómputo AMD pasó de 37 a 70 MB (todo cómputo re-armado). 8 cuadros >100 ms en pleno
  juego (la 43: 0); 15/min en total con los de la carga. Por tramos contra la 43: pesado 33.0 vs 36.2
  FPS (back 80 vs 84 %, syncpt 207 vs 354 ms/s, gpu_read 199 vs 77 ms/s), liviano 50.2 vs 51.9.
  Cierre: solo, 11.5 s (gpu_core.reset 8.5 s). Pendiente: segunda vuelta (cachés calientes) antes
  de juzgar; si el tramo pesado sigue peor y gpu_read sigue al doble, sospechar #1436 (páginas
  escritas por los motores de la GPU siguen rastreadas) o #1441.
- **Segunda vuelta de la 45 (cachés calientes: 3637 de registros, 1 nuevo):** tirones a mitad del
  juego iguales a las builds anteriores (42b 41, 43 33, 45b 34; ≥300 ms: 8, 8, 7; causa texture/other
  en entradas de zona). Por tramos (42b / 43 / 45b): pesado 37.0 / 36.2 / 34.9 FPS, liviano 51.7 /
  52.1 / 49.3; back en liviano 71 / 72 / 79 %. Probable baja de ~2 FPS (~5 %) con los 52 commits
  (una sola vuelta). Lichii midió +1.4 FPS en el agua con #1436. Sospechosos con interruptor:
  #1435/#1439 (EDEN_NX2_TC_MIN_AGE), #1479 (EDEN_NX2_READ_FASTPATH); sin interruptor: #1436, #1441.
  Cierre: 11.9 s, solo.
- **Build 46:** revertidos `31b7d67e` #1479, `6cd99d2e` #1436, `562f6ceb` #1439, `81b202fb` #1435
  (`git show --binary <c> | git apply -R -3 --index`, en ese orden; en vk_rasterizer.cpp
  `InvalidateCachedWrites` se queda con la estructura de #1441/#1460 pero `WriteMemory(addr, size)`
  sin el «tracked» de #1436). #1441 se queda (revertirlo choca con #1460). Al integrar de nuevo:
  volver a revertirlos salvo que una medición diga lo contrario. Si la 46 no recupera los ~2 FPS,
  el siguiente sospechoso es #1441.
- **Vuelta oficial de la 46 (recién abierto, 5.3 min, cachés al día: 3705 de registros):** liviano
  52.2 FPS (43: 52.1, 45b: 49.3) → recuperado; pesado solo 5 ventanas a 34.2 (poco tiempo ahí); las
  ventanas más lentas 34/34/34/38 (43: 27/28/32/34; 45b: 29/32/32/34). Típicos 50.6, 10 % más lento
  37.8 (mejor medido), 5.6 tirones/min (igual). Las lecturas de GPU siguen como en la 45 (~125 ms/s):
  no eran la causa. Cierre solo en 9.4 s (gpu_core.reset 8.5 s). Veredicto: la 46 queda; candidata
  para el amigo.
- **Sesión larga de la 46 con stream (OBS + TikTok Live Studio, 64 min, castillo de Hyrule al
  final; log b46c):** campo 23–42 FPS; castillo desde el min 53: 12–18 FPS con `memory_allocate`
  de hasta 1 s por cuadro; desde el min 56: 7–17 FPS, cada operación del back 5–10× más lenta
  (método 70 → 600 ns, cómputo 15 → 285 µs) con los núcleos poco ocupados; `TrimZeroPages` 8.5 s;
  min 64: `abort()` en GPUBack al fallar un búfer Upload de 64 MiB «over the memory budget»
  (eden_crash.dmp). Presupuesto de VRAM visto ~12 GB de 16 (el stream ocupa ~4). Nunca salió
  «Out of device memory: keeping…»: con #1335 (desborde AMD permitido, desde la build 38) el caché
  de texturas no recibe presión. Aparte: ~8 congeladas de 4–8 s esperando pipelines nuevos (597
  nuevos, zonas nuevas; drivers AMD tardan 5–8 s en algunos).
- **Build 47:** desborde AMD (`VK_AMD_memory_overallocation_behavior`) solo con heap de VRAM
  ≤ 10 GiB; reintento de imagen en memoria del host restaurado (Lichii lo quitó en #1335);
  cherry-pick de Lichii #1580 (`6e887563`) y #1581 (`e633e165`), ambos de memoria del host. Al
  integrar Lichii de nuevo, esos dos ya están. Prueba: castillo con el stream abierto, mirando VRAM.
- En Docked y Handheld el rendimiento de Riddsc es el mismo (límite: procesador); Accurate cuesta
  ~35 % de FPS en Ocarina.
- El rastreador de tirones va encendido por defecto (`EDEN_NX2_STUTTER_TRACE=0` lo apaga): medido,
  no cambia los FPS.
- `tools/satronho/check_satronho.py` corre en `prepare-release` antes de etiquetar: base de los
  guías (`kb_tests/`), novedades, sugerencias (nunca Wait; migración v4 a Bounded), marca visible y
  comparador (registro de prueba en `fixtures/`).
- Pendiente: la neblina blanca del río Zora (vista una vez, en la primera sesión de la 35 en Docked
  con caché nueva) no volvió a aparecer. Si vuelve: probar con `EDEN_NX2_READ_CHECK=8` (compara la
  lectura asíncrona con una síncrona y anota diferencias).
- La caché v2, al migrar desde la v54, precarga variantes de cómputo (clave s0) distintas de las que
  pide el juego (s6): la primera visita a cada zona tras migrar arma las nuevas. Se corrige solo.
