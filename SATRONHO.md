# Satronho NX: cómo se mantiene y qué hemos aprendido

Satronho NX es el fork de Riddsc de **Lichii** (zortos293/Lichii, emulador experimental de Switch 2,
que a su vez viene de Eglantine → Eden → yuzu/Sudachi). Este archivo es la memoria técnica del
proyecto: léelo antes de tocar algo.

## Repositorio y ramas

- `Riddsc/satronho-nx`, rama **`main`** = Satronho NX. La rama `nx2` es una copia vieja de Lichii; no se usa.
- `main` **no comparte historia git con Lichii**: empezó como snapshot de Lichii `588bfa6`.
  `git merge-base main lichii/nx2` sale vacío. Por eso **no se hace merge**; se aplican diffs.
- Último commit de Lichii integrado: **`1a5e995a`** (2026-11-09, #1056; build 35: caché de shaders v2
  #1036, lectura asíncrona #1028, HostTiming #1016, formato .nx #923, rastreador de tirones
  #975/#1039, semáforos #979/#1014/#949). Antes: `cbb512ee` (#932).
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
