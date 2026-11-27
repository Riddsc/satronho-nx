// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import java.io.File
import kotlin.math.roundToInt
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.ui.main.MainActivity
import org.yuzu.yuzu_emu.utils.DirectoryInitialization

/** Satronho NX for Android: shared helpers, colors and preferences of our own features. */
object Satronho {
    private const val PREFS = "satronho"

    // The same colors as the desktop's meter and guides.
    const val VIOLET = 0xFF8B5CF6.toInt()
    const val TEAL = 0xFF4FD1C5.toInt()
    const val GREEN = 0xFF74D14C.toInt()
    const val AMBER = 0xFFFFB23E.toInt()
    const val PINK = 0xFFFF8FB5.toInt()
    const val LILAC = 0xFF9B8CFF.toInt()
    const val RED = 0xFFFF5C7A.toInt()
    const val DIM = 0xFFB9B2CC.toInt()

    val GUIDE_NAMES = linkedMapOf(
        "mecharidds" to "MechaRidds",
        "samy" to "Samy",
        "troya" to "Troya",
        "nono" to "Ñoño",
        "juana" to "Juana"
    )

    /** One of the five guides, as on the desktop (src/yuzu/satronho/guides.cpp). */
    data class Guide(
        val key: String,
        val name: String,
        val roleId: Int,
        val aboutId: Int,
        val color: Int,
        /** Avatar size in dp, after the guide's real size: Ñoño the biggest, Juana the smallest. */
        val sizeDp: Int
    )

    val GUIDES = listOf(
        Guide("mecharidds", "MechaRidds", R.string.satronho_guide_mecharidds_role,
            R.string.satronho_guide_mecharidds_about, GREEN, 60),
        Guide("samy", "Samy", R.string.satronho_guide_samy_role,
            R.string.satronho_guide_samy_about, AMBER, 54),
        Guide("troya", "Troya", R.string.satronho_guide_troya_role,
            R.string.satronho_guide_troya_about, LILAC, 58),
        Guide("nono", "Ñoño", R.string.satronho_guide_nono_role,
            R.string.satronho_guide_nono_about, TEAL, 64),
        Guide("juana", "Juana", R.string.satronho_guide_juana_role,
            R.string.satronho_guide_juana_about, PINK, 40)
    )

    fun guide(key: String): Guide = GUIDES.firstOrNull { it.key == key } ?: GUIDES[0]

    /** MechaRidds is always on; each pet can be turned off, and then MechaRidds answers for it. */
    fun petEnabled(key: String): Boolean = key == "mecharidds" || flag("guide_$key", true)

    fun setPetEnabled(key: String, enabled: Boolean) {
        if (key != "mecharidds") {
            setFlag("guide_$key", enabled)
        }
    }

    /**
     * Who speaks, by how much each one appears (of 1000, same as Satronho Emulation): MechaRidds 300,
     * and the other 700 split 65 % Samy and Troya, 25 % Ñoño, 10 % Juana. Pets turned off are skipped.
     */
    fun pickPet(): String {
        val pets = listOf("mecharidds" to 300, "samy" to 230, "troya" to 225, "nono" to 175, "juana" to 70)
            .filter { petEnabled(it.first) }
        if (pets.isEmpty()) {
            return "mecharidds"
        }
        var roll = kotlin.random.Random.nextInt(pets.sumOf { it.second })
        for ((pet, weight) in pets) {
            if (roll < weight) {
                return pet
            }
            roll -= weight
        }
        return pets.last().first
    }

    /** Who speaks for [key]: the pet itself, or MechaRidds when that pet is off. */
    fun speaker(key: String): String =
        if (key.isNotEmpty() && petEnabled(key)) key else "mecharidds"

    /** The guides, news and knowledge in English for any language but Spanish. */
    fun isSpanish(context: Context): Boolean =
        context.resources.configuration.locales[0].language == "es"

    /** An asset in the app's language: "name_en.json" when not in Spanish, if it exists. */
    fun readLocalizedAsset(context: Context, name: String): String? {
        if (!isSpanish(context)) {
            readAsset(context, name.replace(".json", "_en.json"))?.let { return it }
        }
        return readAsset(context, name)
    }

    fun prefs(context: Context): SharedPreferences =
        context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    fun dp(context: Context, value: Int): Int =
        (value * context.resources.displayMetrics.density).roundToInt()

    fun readAsset(context: Context, name: String): String? =
        try {
            context.assets.open(name).bufferedReader().use { it.readText() }
        } catch (e: Exception) {
            null
        }

    fun guideBitmap(context: Context, guide: String): Bitmap? =
        try {
            context.assets.open("guias/$guide.png").use { BitmapFactory.decodeStream(it) }
        } catch (e: Exception) {
            null
        }

    var guidesEnabled: Boolean
        get() = flag("guides_enabled", true)
        set(value) = setFlag("guides_enabled", value)

    /** Android performance hints (ADPF) for the emulator's frame threads; on by default. */
    var performanceHints: Boolean
        get() = flag("performance_hints", true)
        set(value) = setFlag("performance_hints", value)

    /**
     * Switch 2 on phones with 12 GB or less: the game's memory goes in a file, so Android can page
     * part of it out instead of closing the emulator. Decided by the phone, not a switch; the
     * native side also checks there is storage for it (games under 8 GB never use it).
     */
    private val gameMemoryInFile: Boolean
        get() = org.yuzu.yuzu_emu.utils.MemoryUtil.totalMemory <= 13f * 1024 * 1024 * 1024

    fun applyMemoryOptions() {
        val dir = appContext?.cacheDir?.absolutePath ?: ""
        org.yuzu.yuzu_emu.NativeLibrary.satronhoSetMemoryOptions(
            // Off: punching out zero pages could drop a write the game made between the check
            // and the punch (Ocarina crashed loading data with it on).
            false,
            // Off: a smaller application pool stopped Ocarina with 2162-0001.
            false,
            gameMemoryInFile,
            dir
        )
    }

    var backgroundLogo: Boolean
        get() = logoOpacity > 0
        set(value) {
            logoOpacity = if (value) DEFAULT_LOGO_OPACITY else 0
        }

    const val DEFAULT_LOGO_OPACITY = 15

    /** How visible our logo is behind the games list, 0 (off) to 100 %, as on the desktop. */
    var logoOpacity: Int
        get() = appContext?.let { context ->
            val prefs = prefs(context)
            if (prefs.contains("logo_opacity")) {
                prefs.getInt("logo_opacity", DEFAULT_LOGO_OPACITY)
            } else if (prefs.getBoolean("background_logo", true)) {
                DEFAULT_LOGO_OPACITY
            } else {
                0
            }
        } ?: DEFAULT_LOGO_OPACITY
        set(value) {
            appContext?.let { prefs(it).edit().putInt("logo_opacity", value.coerceIn(0, 100)).apply() }
        }

    /** Our planet with the guides behind the games list, the same image as the desktop's. */
    fun applyBackgroundLogo(view: ImageView) {
        val opacity = logoOpacity
        if (opacity <= 0) {
            view.visibility = View.GONE
            return
        }
        if (view.tag != "satronho_logo") {
            try {
                view.context.assets.open("logo_fondo.png").use {
                    view.setImageBitmap(BitmapFactory.decodeStream(it))
                }
                view.tag = "satronho_logo"
            } catch (e: Exception) {
                // Keep the app icon that the layout already shows.
            }
        }
        view.alpha = opacity / 100f
        view.visibility = View.VISIBLE
    }

    const val CONTROLS_SATRONHO = 0
    const val CONTROLS_CLASSIC = 1
    const val CONTROLS_SUBTLE = 2

    /** How the on-screen controls look: Satronho violet, classic grey or nearly transparent. */
    var controlStyle: Int
        get() = appContext?.let { prefs(it).getInt("control_style", CONTROLS_SATRONHO) } ?: CONTROLS_SATRONHO
        set(value) {
            appContext?.let { prefs(it).edit().putInt("control_style", value).apply() }
        }

    /** Applied to each control's drawable before the overlay draws it. */
    fun styleControl(drawable: android.graphics.drawable.Drawable) {
        when (controlStyle) {
            CONTROLS_CLASSIC -> drawable.colorFilter = android.graphics.ColorMatrixColorFilter(
                android.graphics.ColorMatrix().apply { setSaturation(0f) }
            )
            CONTROLS_SUBTLE -> drawable.alpha = 90
            else -> {}
        }
    }

    /** The summary card when a game closes. */
    var summaryEnabled: Boolean
        get() = flag("summary_enabled", true)
        set(value) = setFlag("summary_enabled", value)

    /** Shares the newest log: this session's if a game ran, otherwise the last session's. */
    fun shareLatestLog(activity: Activity) {
        val base = DirectoryInitialization.userDirectory
        val newest = Sesiones.files().firstOrNull()
        val current = base?.let { File(it, "log/satronho_log.txt") }
        val file = listOfNotNull(newest, current?.takeIf { it.exists() && it.length() > 0 })
            .maxByOrNull { it.lastModified() }
        if (file == null) {
            Toast.makeText(activity, R.string.share_log_missing, Toast.LENGTH_SHORT).show()
            return
        }
        Sesiones.share(activity, file)
    }

    /** Opens the Satronho screen from a launcher shortcut. */
    const val ACTION_OPEN = "dev.satronho.nx.ABRIR_SATRONHO"

    fun openIntent(context: Context): Intent =
        Intent(context, MainActivity::class.java).setAction(ACTION_OPEN)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP)

    var meterEnabled: Boolean
        get() = flag("meter_enabled", true)
        set(value) = setFlag("meter_enabled", value)

    private var appContext: Context? = null

    /** Called once from the application: the flags above read from it. */
    fun init(context: Context) {
        appContext = context.applicationContext
    }

    private fun flag(key: String, default: Boolean): Boolean =
        appContext?.let { prefs(it).getBoolean(key, default) } ?: default

    private fun setFlag(key: String, value: Boolean) {
        appContext?.let { prefs(it).edit().putBoolean(key, value).apply() }
    }

    /** A small rounded label, as the desktop's Nuevo / Mejora / Arreglo tags. */
    fun styleTag(view: TextView, color: Int) {
        val context = view.context
        view.setTextColor(0xFF0F0D16.toInt())
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
        view.paint.isFakeBoldText = true
        view.setPadding(dp(context, 8), dp(context, 2), dp(context, 8), dp(context, 2))
        view.background = GradientDrawable().apply {
            cornerRadius = dp(context, 10).toFloat()
            setColor(color)
        }
    }

    fun setVisible(view: View, visible: Boolean) {
        view.visibility = if (visible) View.VISIBLE else View.GONE
    }
}
