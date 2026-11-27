// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import android.util.TypedValue
import android.view.View
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.materialswitch.MaterialSwitch
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import org.yuzu.yuzu_emu.utils.NativeConfig

/**
 * «Configuraciones sugeridas» for the phone: its graphics family (Adreno, Mali on Tensor and
 * Dimensity, Xclipse) and RAM pick three profiles, one recommended, applied with one tap.
 */
object Sugeridas {
    enum class Family { AdrenoHigh, Adreno, Mali, Xclipse }

    // ResolutionSetup indices (common/settings_enums.h).
    internal const val RES_1_2 = 1
    internal const val RES_3_4 = 2
    internal const val RES_1X = 3
    internal const val RES_3_2 = 5

    // ScalingFilter indices.
    internal const val FILTER_BILINEAR = 1
    internal const val FILTER_FSR = 6

    data class Profile(
        val nameId: Int,
        val descriptionId: Int,
        val resolution: Int,
        val filter: Int
    )

    data class Device(val family: Family, val label: String, val ramGb: Int)

    fun detect(context: Context): Device {
        val maker = Build.SOC_MANUFACTURER.orEmpty()
        val model = Build.SOC_MODEL.orEmpty()
        val adreno = GpuDriverHelper.isAdrenoGpu()
        val family = when {
            adreno -> {
                val number = Regex("SM(\\d{4})").find(model.uppercase())
                    ?.groupValues?.get(1)?.toIntOrNull() ?: 0
                if (number >= 8550) Family.AdrenoHigh else Family.Adreno
            }
            maker.equals("Samsung", true) &&
                model.lowercase().let { it.startsWith("s5e99") } -> Family.Xclipse
            else -> Family.Mali
        }
        val gpu = when (family) {
            Family.AdrenoHigh, Family.Adreno -> "Adreno"
            Family.Xclipse -> "Xclipse"
            Family.Mali -> "Mali"
        }
        val socName = when {
            maker.equals("Google", true) -> "Google Tensor"
            maker.isNotEmpty() -> maker
            else -> Build.HARDWARE
        }
        val memory = ActivityManager.MemoryInfo()
        (context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager)
            .getMemoryInfo(memory)
        val ramGb = ((memory.totalMem + (1L shl 29)) shr 30).toInt()
        val label = context.getString(
            R.string.satronho_suggested_device,
            if (model.isNotEmpty()) "$socName ($model)" else socName,
            gpu,
            ramGb
        )
        return Device(family, label, ramGb)
    }

    private fun profiles(family: Family): List<Profile> = when (family) {
        Family.AdrenoHigh, Family.Xclipse -> listOf(
            Profile(R.string.satronho_profile_performance, R.string.satronho_profile_performance_description, RES_3_4, FILTER_BILINEAR),
            Profile(R.string.satronho_profile_balanced, R.string.satronho_profile_balanced_description, RES_1X, FILTER_BILINEAR),
            Profile(R.string.satronho_profile_quality, R.string.satronho_profile_quality_description, RES_3_2, FILTER_FSR)
        )
        Family.Adreno, Family.Mali -> listOf(
            Profile(R.string.satronho_profile_performance, R.string.satronho_profile_performance_description, RES_1_2, FILTER_BILINEAR),
            Profile(R.string.satronho_profile_balanced, R.string.satronho_profile_balanced_description, RES_3_4, FILTER_BILINEAR),
            Profile(R.string.satronho_profile_quality, R.string.satronho_profile_quality_description, RES_1X, FILTER_FSR)
        )
    }

    /** Index of the recommended profile: one step lower with under 8 GB of RAM. */
    private fun recommended(device: Device): Int {
        val base = when (device.family) {
            Family.AdrenoHigh, Family.Xclipse -> 1
            Family.Adreno, Family.Mali -> 0
        }
        return if (device.ramGb < 8) maxOf(base - 1, 0) else base
    }

    fun apply(profile: Profile) {
        IntSetting.RENDERER_RESOLUTION.setInt(profile.resolution)
        IntSetting.RENDERER_SCALING_FILTER.setInt(profile.filter)
        IntSetting.RENDERER_ACCURACY.setInt(0)
        IntSetting.RENDERER_VRAM_USAGE_MODE.setInt(0)
        IntSetting.RENDERER_ANTI_ALIASING.setInt(0)
        BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.setBoolean(true)
        BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.setBoolean(true)
        NativeConfig.saveGlobalConfig()
    }

    /** The view and what «Aplicar» does with the profile picked in it. */
    class Content(val view: View, val apply: () -> Unit)

    fun buildContent(context: Context): Content {
        val device = detect(context)
        val list = profiles(device.family)
        val best = recommended(device)
        val pad = Satronho.dp(context, 20)
        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, Satronho.dp(context, 8), pad, 0)
        }
        root.addView(TextView(context).apply {
            text = device.label
            setTextColor(Satronho.TEAL)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
        })
        if (device.family == Family.Mali) {
            root.addView(TextView(context).apply {
                text = context.getString(R.string.satronho_suggested_mali_note)
                setTextColor(Satronho.DIM)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
                setPadding(0, Satronho.dp(context, 6), 0, 0)
            })
        }
        val group = RadioGroup(context).apply {
            setPadding(0, Satronho.dp(context, 8), 0, 0)
        }
        list.forEachIndexed { index, profile ->
            val name = context.getString(profile.nameId) +
                if (index == best) " · " + context.getString(R.string.satronho_recommended) else ""
            group.addView(RadioButton(context).apply {
                id = 1000 + index
                text = name
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            })
            group.addView(TextView(context).apply {
                text = context.getString(profile.descriptionId)
                setTextColor(Satronho.DIM)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
                setPadding(Satronho.dp(context, 32), 0, 0, Satronho.dp(context, 8))
            })
        }
        group.check(1000 + best)
        root.addView(group)
        root.addView(TextView(context).apply {
            text = context.getString(R.string.satronho_suggested_footer)
            setTextColor(Satronho.DIM)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setPadding(0, Satronho.dp(context, 4), 0, 0)
        })

        // Satronho NX: the phone boost (Android performance hints), when the phone has it.
        if (NativeLibrary.satronhoPerformanceHintsSupported()) {
            val row = LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = android.view.Gravity.CENTER_VERTICAL
                setPadding(0, Satronho.dp(context, 12), 0, 0)
            }
            val texts = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }
            texts.addView(TextView(context).apply {
                text = context.getString(R.string.satronho_boost)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            })
            texts.addView(TextView(context).apply {
                text = context.getString(R.string.satronho_boost_description)
                setTextColor(Satronho.DIM)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            })
            row.addView(texts, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            row.addView(MaterialSwitch(context).apply {
                isChecked = Satronho.performanceHints
                contentDescription = context.getString(R.string.satronho_boost)
                setOnCheckedChangeListener { _, checked -> Satronho.performanceHints = checked }
            })
            root.addView(row)
        }

        // Satronho NX: the memory of Switch 2 games is set up by the app for this phone.
        root.addView(TextView(context).apply {
            text = context.getString(R.string.satronho_memory_auto)
            setTextColor(Satronho.DIM)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            setPadding(0, Satronho.dp(context, 12), 0, 0)
        })

        val applyPicked = {
            val index = (group.checkedRadioButtonId - 1000).coerceIn(0, list.size - 1)
            apply(list[index])
            Toast.makeText(
                context,
                context.getString(R.string.satronho_suggested_applied, context.getString(list[index].nameId)),
                Toast.LENGTH_SHORT
            ).show()
        }
        return Content(ScrollView(context).apply { addView(root) }, applyPicked)
    }

    fun show(context: Context) {
        val content = buildContent(context)
        MaterialAlertDialogBuilder(context)
            .setTitle(R.string.satronho_suggested)
            .setView(content.view)
            .setPositiveButton(R.string.satronho_apply) { _, _ -> content.apply() }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }
}
