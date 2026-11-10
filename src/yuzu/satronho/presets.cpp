// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <thread>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include "common/cpu_features.h"
#include "common/fs/path_util.h"
#include "common/memory_detect.h"
#include "common/settings.h"
#include "qt_common/util/vk.h"
#include "yuzu/satronho/presets.h"

namespace Satronho {
namespace {

QString Tr(const char* text) {
    return QCoreApplication::translate("Satronho", text);
}

constexpr u64 GiB = 1ULL << 30;

QString GiBText(u64 bytes) {
    return QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / static_cast<double>(GiB), 0,
                                       'f', 0);
}

QString OnOff(bool value) {
    return value ? Tr("Activado") : Tr("Desactivado");
}

QString ResolutionText(Settings::ResolutionSetup value) {
    using R = Settings::ResolutionSetup;
    switch (value) {
    case R::Res1_4X: return QStringLiteral("0.25x");
    case R::Res1_2X: return QStringLiteral("0.5x");
    case R::Res3_4X: return QStringLiteral("0.75x");
    case R::Res1X: return QStringLiteral("1x");
    case R::Res5_4X: return QStringLiteral("1.25x");
    case R::Res3_2X: return QStringLiteral("1.5x");
    case R::Res2X: return QStringLiteral("2x");
    case R::Res3X: return QStringLiteral("3x");
    case R::Res4X: return QStringLiteral("4x");
    default: return QStringLiteral("5x+");
    }
}

QString FilterText(Settings::ScalingFilter value) {
    using F = Settings::ScalingFilter;
    switch (value) {
    case F::Fsr: return QStringLiteral("AMD FSR");
    case F::ScaleForce: return QStringLiteral("ScaleForce");
    case F::Bilinear: return Tr("Bilineal");
    case F::Bicubic: return Tr("Bicúbico");
    case F::NearestNeighbor: return Tr("Vecino más cercano");
    case F::Fsr3: return QStringLiteral("AMD FSR 3");
    default: return Tr("Otro");
    }
}

QString AaText(Settings::AntiAliasing value) {
    switch (value) {
    case Settings::AntiAliasing::Smaa: return QStringLiteral("SMAA");
    case Settings::AntiAliasing::Fxaa: return QStringLiteral("FXAA");
    default: return Tr("Ninguno");
    }
}

QString AccuracyText(Settings::GpuAccuracy value) {
    return value == Settings::GpuAccuracy::Low ? QStringLiteral("Fast")
                                               : QStringLiteral("Accurate");
}

QString DrawsText(Settings::NativeAsyncDraws value) {
    switch (value) {
    case Settings::NativeAsyncDraws::Wait: return QStringLiteral("Wait");
    case Settings::NativeAsyncDraws::Bounded: return QStringLiteral("Bounded");
    default: return QStringLiteral("Skip");
    }
}

QString AstcDecodeText(Settings::AstcDecodeMode value) {
    switch (value) {
    case Settings::AstcDecodeMode::Gpu: return QStringLiteral("GPU");
    case Settings::AstcDecodeMode::Cpu: return QStringLiteral("CPU");
    default: return QStringLiteral("CPU Asynchronous");
    }
}

QString AstcRecompressionText(Settings::AstcRecompression value) {
    switch (value) {
    case Settings::AstcRecompression::Bc3: return QStringLiteral("BC3");
    case Settings::AstcRecompression::Bc1: return QStringLiteral("BC1");
    default: return QStringLiteral("Uncompressed");
    }
}

QString VramText(Settings::VramUsageMode value) {
    return value == Settings::VramUsageMode::Aggressive ? QStringLiteral("Aggressive")
                                                        : QStringLiteral("Conservative");
}

QString PrecacheText(Settings::PrecacheShaders value) {
    switch (value) {
    case Settings::PrecacheShaders::Ask: return QStringLiteral("Ask");
    case Settings::PrecacheShaders::Always: return QStringLiteral("Always");
    default: return QStringLiteral("Off");
    }
}

QString FamilyName(GpuFamily family) {
    switch (family) {
    case GpuFamily::AmdRdna4: return QStringLiteral("AMD Radeon RX 9000 (RDNA 4)");
    case GpuFamily::AmdRdna3: return QStringLiteral("AMD Radeon RX 7000 (RDNA 3)");
    case GpuFamily::AmdRdna2: return QStringLiteral("AMD Radeon RX 6000 (RDNA 2)");
    case GpuFamily::AmdOlder: return Tr("AMD Radeon RX 5000, Vega o anterior");
    case GpuFamily::NvBlackwell: return QStringLiteral("NVIDIA GeForce RTX 50");
    case GpuFamily::NvAda: return QStringLiteral("NVIDIA GeForce RTX 40");
    case GpuFamily::NvAmpere: return QStringLiteral("NVIDIA GeForce RTX 30");
    case GpuFamily::NvTuring: return QStringLiteral("NVIDIA GeForce RTX 20 / GTX 16");
    case GpuFamily::NvOlder: return Tr("NVIDIA GeForce GTX 10 o anterior");
    case GpuFamily::AmdIntegrated: return Tr("Gráficos integrados de AMD (Ryzen)");
    case GpuFamily::NvWorkstation: return Tr("NVIDIA RTX profesional / Quadro");
    case GpuFamily::IntelArc: return QStringLiteral("Intel Arc (A / B)");
    case GpuFamily::IntelArcIntegrated: return Tr("Intel Arc integrada (Core Ultra)");
    case GpuFamily::IntelIntegrated: return Tr("Gráficos integrados de Intel (UHD / Iris Xe)");
    case GpuFamily::Unknown: break;
    }
    return Tr("Tarjeta no reconocida");
}

bool IsOlderFamily(GpuFamily family) {
    return family == GpuFamily::AmdOlder || family == GpuFamily::NvOlder ||
           family == GpuFamily::IntelIntegrated;
}

bool IsIntegrated(GpuFamily family) {
    return family == GpuFamily::AmdIntegrated || family == GpuFamily::IntelArcIntegrated ||
           family == GpuFamily::IntelIntegrated;
}

template <typename T, bool ranged>
void AddChange(Suggestion& suggestion, Settings::SwitchableSetting<T, ranged>& setting,
               T value, QString label, QString location, QString current, QString suggested,
               QString reason) {
    const bool differs = setting.GetValue(true) != value;
    suggestion.changes.push_back(SettingChange{
        .label = std::move(label),
        .location = std::move(location),
        .current = std::move(current),
        .suggested = std::move(suggested),
        .reason = std::move(reason),
        .differs = differs,
        .apply = [&setting, value] { setting.SetValue(value); },
        .ini_key = setting.GetLabel().c_str(),
        .ini_section = "Renderer",
    });
}

// Switches one per-game file's own values of `keys` to the global ones. Returns whether it
// changed. The files keep "<key>\global=false" for a setting the game overrides.
bool UseGlobalIn(const QString& path, const QStringList& keys) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray data = file.readAll();
    file.close();
    bool changed = false;
    for (const QString& key : keys) {
        const QRegularExpression pattern(
            QStringLiteral("(^|\\n)(%1\\\\{1,2}global\\s*=\\s*)false").arg(
                QRegularExpression::escape(key)));
        QString text = QString::fromUtf8(data);
        const QString replaced = QString(text).replace(pattern, QStringLiteral("\\1\\2true"));
        if (replaced != text) {
            data = replaced.toUtf8();
            changed = true;
        }
    }
    if (!changed || !file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(data);
    return true;
}

} // Anonymous namespace

GpuFamily ClassifyGpu(const QString& name, u32 vendor_id) {
    const QString n = name.toUpper();
    const auto has = [&](const char* pattern) {
        return QRegularExpression(QString::fromLatin1(pattern)).match(n).hasMatch();
    };
    if (vendor_id == 0x1002 || n.contains(QStringLiteral("RADEON")) ||
        n.contains(QStringLiteral("AMD"))) {
        if (has("RX\\s*9\\d{3}")) return GpuFamily::AmdRdna4;
        if (has("RX\\s*7\\d{3}")) return GpuFamily::AmdRdna3;
        if (has("RX\\s*6\\d{3}")) return GpuFamily::AmdRdna2;
        if (has("RX\\s*5\\d{3}") || has("RX\\s*VEGA") || has("RX\\s*[45]\\d{2}\\b") ||
            has("VEGA\\s*(56|64)") || has("RADEON\\s*VII") || has("R9\\s") || has("R7\\s"))
            return GpuFamily::AmdOlder;
        // Ryzen integrated: "Radeon 780M Graphics", "Radeon(TM) Graphics", "Radeon 8060S".
        if (has("\\b\\d{3,4}[MS]\\b") || n.contains(QStringLiteral("GRAPHICS")) ||
            has("VEGA\\s*\\d+\\b"))
            return GpuFamily::AmdIntegrated;
        return GpuFamily::Unknown;
    }
    if (vendor_id == 0x10DE || n.contains(QStringLiteral("NVIDIA")) ||
        n.contains(QStringLiteral("GEFORCE"))) {
        if (has("RTX\\s*PRO") || has("RTX\\s*A\\d{3,4}") || has("RTX\\s*\\d{4}\\s*ADA") ||
            n.contains(QStringLiteral("QUADRO")) || has("\\bT\\d{3,4}\\b"))
            return GpuFamily::NvWorkstation;
        if (has("RTX\\s*50\\d{2}")) return GpuFamily::NvBlackwell;
        if (has("RTX\\s*40\\d{2}")) return GpuFamily::NvAda;
        if (has("RTX\\s*30\\d{2}")) return GpuFamily::NvAmpere;
        if (has("RTX\\s*20\\d{2}") || has("GTX\\s*16\\d{2}")) return GpuFamily::NvTuring;
        if (has("GTX\\s*10\\d{2}") || has("GTX\\s*9\\d{2}") || has("GT\\s*\\d{3,4}") ||
            has("MX\\s*\\d{3}"))
            return GpuFamily::NvOlder;
        return GpuFamily::Unknown;
    }
    if (vendor_id == 0x8086 || n.contains(QStringLiteral("INTEL"))) {
        if (has("ARC.*\\b[AB]\\d{3}")) return GpuFamily::IntelArc;  // "Arc A770", "Arc B580"
        if (n.contains(QStringLiteral("ARC"))) return GpuFamily::IntelArcIntegrated;
        return GpuFamily::IntelIntegrated;
    }
    return GpuFamily::Unknown;
}

Hardware DetectHardware(const std::vector<VkDeviceInfo::Record>& records) {
    Hardware hardware;
    if (!records.empty()) {
        const size_t index = std::min<size_t>(
            static_cast<size_t>(Settings::values.vulkan_device.GetValue(true)), records.size() - 1);
        const VkDeviceInfo::Record& record = records[index];
        hardware.gpu_name = QString::fromStdString(
            record.device_name.empty() ? record.name : record.device_name);
        hardware.vendor_id = record.vendor_id;
        hardware.vram_bytes = record.device_local_bytes;
    }
#ifdef ARCHITECTURE_x86_64
    hardware.cpu_name = QString::fromLatin1(Common::g_cpu_caps.cpu_string,
                                            sizeof(Common::g_cpu_caps.cpu_string))
                            .section(QChar{}, 0, 0)
                            .trimmed();
#endif
    hardware.cpu_threads = std::thread::hardware_concurrency();
    hardware.ram_bytes = Common::GetMemInfo().TotalPhysicalMemory;
    return hardware;
}

Suggestion Suggest(const Hardware& hardware) {
    Suggestion suggestion;
    suggestion.family = ClassifyGpu(hardware.gpu_name, hardware.vendor_id);
    suggestion.family_name = FamilyName(suggestion.family);
    const u64 vram = hardware.vram_bytes;
    const bool big_vram = vram >= 15 * GiB;     // 16 GB cards report a little under 16 GiB
    const bool huge_vram = vram >= 20 * GiB;
    const bool integrated = IsIntegrated(suggestion.family);
    const bool small_vram = !integrated && vram > 0 && vram < 11 * GiB;
    auto& v = Settings::values;

    // The settings: same idea on every card (fidelity first), sized by video memory.
    AddChange(suggestion, v.gpu_accuracy, Settings::GpuAccuracy::Low, QStringLiteral("GPU Mode"),
              Tr("Graphics → Advanced"), AccuracyText(v.gpu_accuracy.GetValue(true)),
              QStringLiteral("Fast"),
              Tr("La mayoría de juegos se ve bien en Fast y Ocarina va cerca de un tercio más "
                 "rápido que en Accurate."));
    AddChange(suggestion, v.resolution_setup, Settings::ResolutionSetup::Res1X,
              QStringLiteral("Resolution"), Tr("Graphics"),
              ResolutionText(v.resolution_setup.GetValue(true)), QStringLiteral("1x"),
              big_vram ? Tr("Como en la consola y con el mejor rendimiento. Con tu memoria de "
                            "video puedes subir a 1.5x si quieres más detalle.")
                       : Tr("Como en la consola, y lo que mejor rinde. Subir cuesta memoria de "
                            "video y FPS."));
    AddChange(suggestion, v.scaling_filter, Settings::ScalingFilter::Fsr,
              QStringLiteral("Window Adapting Filter"), Tr("Graphics"),
              FilterText(v.scaling_filter.GetValue(true)), QStringLiteral("AMD FSR"),
              Tr("Agranda la imagen a tu pantalla con buena nitidez, en cualquier marca de "
                 "tarjeta."));
    AddChange(suggestion, v.anti_aliasing, Settings::AntiAliasing::Smaa,
              QStringLiteral("Anti-Aliasing Method"), Tr("Graphics"),
              AaText(v.anti_aliasing.GetValue(true)), QStringLiteral("SMAA"),
              Tr("El suavizado de bordes de mejor calidad."));
    AddChange(suggestion, v.native_async_draws, Settings::NativeAsyncDraws::Bounded,
              QStringLiteral("Switch 2 draws with unbuilt pipelines"), Tr("Graphics → Advanced"),
              DrawsText(v.native_async_draws.GetValue(true)), QStringLiteral("Bounded"),
              Tr("Espera unos milisegundos por cuadro a que cada shader nuevo esté listo y, si "
                 "tarda más, sigue sin congelar el juego. Wait congela el juego mientras se "
                 "prepara cada shader nuevo, y Skip puede dejar efectos faltantes o colores "
                 "raros."));
    AddChange(suggestion, v.precache_shaders, Settings::PrecacheShaders::Ask,
              QStringLiteral("Precache shaders before game start"), Tr("Graphics → Advanced"),
              PrecacheText(v.precache_shaders.GetValue(true)), QStringLiteral("Ask"),
              Tr("Te ofrece preparar los shaders en la pantalla de carga, para que queden menos "
                 "por preparar mientras juegas."));
    AddChange(suggestion, v.use_asynchronous_shaders, true,
              QStringLiteral("Enable asynchronous shader compilation"), Tr("Graphics → Extra"),
              OnOff(v.use_asynchronous_shaders.GetValue(true)), OnOff(true),
              Tr("Prepara los shaders en segundo plano."));
    AddChange(suggestion, v.use_disk_shader_cache, true,
              QStringLiteral("Use persistent pipeline cache"), Tr("Graphics → Advanced"),
              OnOff(v.use_disk_shader_cache.GetValue(true)), OnOff(true),
              Tr("Guarda los shaders para que la próxima vez carguen rápido."));
    AddChange(suggestion, v.accelerate_astc, Settings::AstcDecodeMode::Gpu,
              QStringLiteral("ASTC Decoding Method"), Tr("Graphics → Advanced"),
              AstcDecodeText(v.accelerate_astc.GetValue(true)), QStringLiteral("GPU"),
              Tr("La tarjeta traduce las texturas del juego, que es lo más rápido."));
    const auto recompression =
        huge_vram ? Settings::AstcRecompression::Uncompressed : Settings::AstcRecompression::Bc3;
    AddChange(suggestion, v.astc_recompression, recompression,
              QStringLiteral("ASTC Recompression Method"), Tr("Graphics → Advanced"),
              AstcRecompressionText(v.astc_recompression.GetValue(true)),
              AstcRecompressionText(recompression),
              huge_vram ? Tr("Con tanta memoria de video, las texturas pueden quedar sin "
                             "recomprimir, con la mejor calidad.")
                        : Tr("Ocupa mucha menos memoria de video. Con menos de unos 20 GB, "
                             "Satronho la usa igual aunque elijas Uncompressed."));
    const auto vram_mode =
        huge_vram ? Settings::VramUsageMode::Aggressive : Settings::VramUsageMode::Conservative;
    AddChange(suggestion, v.vram_usage_mode, vram_mode, QStringLiteral("VRAM Usage Mode"),
              Tr("Graphics → Advanced"), VramText(v.vram_usage_mode.GetValue(true)),
              VramText(vram_mode),
              huge_vram ? Tr("Tu tarjeta tiene memoria de sobra: Aggressive la aprovecha.")
                       : Tr("Deja margen para que la memoria de video no se llene."));

    // The paragraph and notes.
    const QString vram_text = vram > 0 ? GiBText(vram) : Tr("memoria de video desconocida");
    if (hardware.gpu_name.isEmpty()) {
        suggestion.summary = Tr("No encontré una tarjeta gráfica con Vulkan. Revisa que el driver "
                                "de tu tarjeta esté instalado y actualizado.");
    } else if (IsOlderFamily(suggestion.family)) {
        suggestion.summary =
            Tr("Tu %1 (%2) es de una generación que puede no mover bien los juegos de Switch 2. "
               "Te dejo la configuración más liviana; si va lento o no arranca, la tarjeta es el "
               "límite.")
                .arg(hardware.gpu_name, vram_text);
    } else if (integrated) {
        suggestion.summary =
            Tr("Tu %1 son gráficos integrados: comparten la RAM del PC y Switch 2 les exige "
               "mucho. Te dejo la configuración más liviana; espera pocos FPS en zonas grandes.")
                .arg(hardware.gpu_name);
    } else if (suggestion.family == GpuFamily::Unknown) {
        suggestion.summary = Tr("No reconozco la generación de tu %1 (%2), así que te dejo la "
                                "configuración general, que funciona en la mayoría de tarjetas.")
                                 .arg(hardware.gpu_name, vram_text);
    } else {
        suggestion.summary =
            Tr("Tu %1 (%2) es de la familia %3. Esta configuración prioriza que se vea como en "
               "la consola y rinde bien en ella.")
                .arg(hardware.gpu_name, vram_text, suggestion.family_name);
    }
    if (integrated) {
        suggestion.notes.push_back(
            Tr("Con gráficos integrados conviene tener 32 GB de RAM en doble canal, porque la "
               "tarjeta usa esa misma memoria, y cerrar todo lo demás."));
    }
    if (small_vram) {
        suggestion.notes.push_back(
            Tr("Tu tarjeta tiene poca memoria de video para Switch 2: quédate en 1x y evita "
               "subir la resolución."));
    }
    if (suggestion.family == GpuFamily::NvAda || suggestion.family == GpuFamily::NvBlackwell ||
        suggestion.family == GpuFamily::NvAmpere || suggestion.family == GpuFamily::NvTuring) {
        suggestion.notes.push_back(
            Tr("En tarjetas RTX existe «Use NVIDIA DLSS in place of a game's DLSS» "
               "(experimental). No lo activo por ti porque necesita descargar los archivos de "
               "NVIDIA desde esa misma sección."));
    }
    // Ocarina's FSR 3.1 / FSR 4 (Lichii #1060-#1062): measured on an RX 9070 XT, not suggested yet.
    const bool fsr4_family = suggestion.family == GpuFamily::AmdRdna4 ||
                             suggestion.family == GpuFamily::AmdRdna3 ||
                             suggestion.family == GpuFamily::AmdRdna2;
    if (fsr4_family) {
        suggestion.notes.push_back(
            Tr("Tu tarjeta puede usar AMD FSR 4 en Ocarina («Graphics»), pero por ahora no lo "
               "recomiendo: en nuestras pruebas con una RX 9070 XT congeló el juego unos 6 "
               "segundos al arrancar, dibuja a 4K aunque tu monitor sea menor (se ve más fino, "
               "pero la tarjeta trabaja de más) y dio algunos FPS menos y más tirones. Fueron "
               "pocos minutos de prueba: hacen falta más antes de recomendarlo."));
    } else {
        suggestion.notes.push_back(
            Tr("Para Ocarina existe «Use AMD FSR 3.1 upscaling in Ocarina of Time» («Graphics»). "
               "No lo activo por ti: todavía no lo hemos probado lo suficiente para recomendarlo."));
    }
    if (hardware.cpu_threads > 0) {
        if (hardware.cpu_threads < 12) {
            suggestion.notes.push_back(
                Tr("Tu procesador tiene %1 hilos. En Switch 2 el procesador es lo que más limita: "
                   "cierra programas en segundo plano mientras juegas.")
                    .arg(hardware.cpu_threads));
        } else {
            suggestion.notes.push_back(
                Tr("Tu procesador (%1 hilos) va bien. En Ocarina es lo que más pesa en los FPS, "
                   "más que la tarjeta.")
                    .arg(hardware.cpu_threads));
        }
    }
    if (hardware.ram_bytes > 0) {
        const u64 ram = hardware.ram_bytes;
        if (ram < 15 * GiB) {
            suggestion.notes.push_back(
                Tr("Tienes %1 de RAM. Los juegos de Switch 2 usan 12 GB de memoria emulada, "
                   "así que puede ir mal: cierra todo lo demás.")
                    .arg(GiBText(ram)));
        } else if (ram < 30 * GiB) {
            suggestion.notes.push_back(Tr("Tienes %1 de RAM: alcanza, pero conviene cerrar el "
                                          "navegador mientras juegas.")
                                           .arg(GiBText(ram)));
        } else {
            suggestion.notes.push_back(Tr("Tienes %1 de RAM: de sobra.").arg(GiBText(ram)));
        }
    }
    return suggestion;
}

QString GenerationGuideHtml() {
    struct Row {
        const char* family;
        const char* text;
    };
    static const Row rows[] = {
        {"AMD Radeon RX 9000 (RDNA 4)",
         "1x con AMD FSR o ScaleForce y SMAA; puede subir a 1.5x o 2x (en una RX 9070 XT, 2x "
         "costó cerca de un 20 % de FPS). Memoria de video en Conservative y Bounded. Probada a "
         "fondo. FSR 4 para Ocarina funciona, pero aún no lo recomendamos: congela ~6 s al "
         "arrancar, dibuja a 4K y dio algunos FPS menos en pruebas cortas."},
        {"AMD Radeon RX 7000 (RDNA 3)",
         "Como la RX 9000: 1x con FSR y SMAA, Conservative y Bounded. Con 16 GB o más, hasta "
         "1.5x."},
        {"AMD Radeon RX 6000 (RDNA 2)",
         "1x, texturas en BC3, memoria en Conservative, Bounded y precache de "
         "shaders. Evita Skip: deja colores raros mientras se preparan los shaders. Probada en una "
         "RX 6700 XT."},
        {"AMD RX 5000, Vega, Polaris",
         "Generaciones que pueden no mover bien Switch 2. Lo más liviano: 1x, BC3, Conservative."},
        {"NVIDIA GeForce RTX 50 y 40",
         "1x con FSR y SMAA; con 16 GB puede subir a 1.5x. Opcional: DLSS de NVIDIA en lugar del "
         "escalado del juego (experimental)."},
        {"NVIDIA GeForce RTX 30 y 20",
         "1x, BC3 y memoria en Conservative en las de 8 a 12 GB. Bounded y precache de shaders."},
        {"NVIDIA GTX 16, GTX 10 o anteriores",
         "La GTX 16 es lo mínimo razonable; la GTX 10 puede no funcionar. 1x, BC3, Conservative."},
        {"NVIDIA RTX profesional / Quadro",
         "Se configuran como la GeForce de su misma generación, según su memoria de video."},
        {"Intel Arc A y B (dedicadas)",
         "Sin pruebas propias todavía. Empieza con la configuración general: 1x, FSR, SMAA, BC3, "
         "Bounded y memoria en Conservative."},
        {"Gráficos integrados de AMD (Radeon 680M, 780M, 890M)",
         "Comparten la RAM del PC. Lo más liviano: 1x, BC3, Conservative, y 32 GB de RAM en doble "
         "canal. Espera pocos FPS en zonas grandes."},
        {"Intel Arc integrada (Core Ultra) y UHD / Iris Xe",
         "La Arc integrada puede arrancar los juegos con lo más liviano; UHD e Iris Xe "
         "probablemente no alcanzan para Switch 2."},
    };
    QString html = QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"8\">");
    for (const Row& row : rows) {
        html += QStringLiteral("<tr><td width=\"34%\" valign=\"top\"><b>%1</b></td>"
                               "<td valign=\"top\">%2</td></tr>")
                    .arg(Tr(row.family).toHtmlEscaped(), Tr(row.text).toHtmlEscaped());
    }
    html += QStringLiteral("</table><p><b>%1</b> %2</p><p><b>%3</b> %4</p>")
                .arg(Tr("Procesador:"),
                     Tr("6 a 8 núcleos con 12 hilos o más. En Switch 2 pesa más que la tarjeta; "
                        "deja GPU Mode en Fast.").toHtmlEscaped(),
                     Tr("RAM:"),
                     Tr("16 GB mínimo (cierra el navegador); 32 GB o más, tranquilo.")
                         .toHtmlEscaped());
    return html;
}

int ApplySuggestion(const Suggestion& suggestion, bool per_game) {
    QStringList keys;
    for (const SettingChange& change : suggestion.changes) {
        if (change.differs && change.apply) {
            change.apply();
        }
        if (change.ini_key != nullptr) {
            keys.push_back(QString::fromLatin1(change.ini_key));
        }
    }
    if (!per_game) {
        return 0;
    }
    int changed = 0;
    const QDir custom(QString::fromStdString(Common::FS::PathToUTF8String(
                          Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir))) +
                      QStringLiteral("/custom"));
    for (const QString& name : custom.entryList({QStringLiteral("*.ini")}, QDir::Files)) {
        if (UseGlobalIn(custom.filePath(name), keys)) {
            ++changed;
        }
    }
    return changed;
}

} // namespace Satronho
