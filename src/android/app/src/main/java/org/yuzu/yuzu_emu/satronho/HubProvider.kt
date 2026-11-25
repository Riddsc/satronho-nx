// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.content.ComponentName
import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.content.pm.PackageManager
import android.database.Cursor
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Bundle
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FilenameFilter
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.GameMetadata
import org.yuzu.yuzu_emu.utils.Log
import org.yuzu.yuzu_emu.utils.NativeConfig

/**
 * What Satronho Emulation (the hub) asks of Satronho NX when NX is installed as its Switch
 * emulator: status (keys, firmware), a game's real title and icon, installing the person's own
 * keys and firmware, and hiding or showing NX's launcher icon. Only the hub may call it.
 *
 * Called with ContentResolver.call(content://dev.satronho.nx.hub, method, arg, extras).
 */
class HubProvider : ContentProvider() {
    override fun onCreate(): Boolean = true

    override fun call(method: String, arg: String?, extras: Bundle?): Bundle? {
        if (callingPackage !in HUB_PACKAGES) {
            Log.warning("[Satronho hub] refused call '$method' from $callingPackage")
            return null
        }
        val ctx = context ?: return null
        return try {
            when (method) {
                "estado" -> status(ctx)
                "info" -> arg?.let { info(it) }
                "claves" -> arg?.let { keys(it) }
                "firmware" -> arg?.let { firmware(ctx, it) }
                "ocultar" -> launcher(ctx, false)
                "mostrar" -> launcher(ctx, true)
                else -> null
            }
        } catch (e: Throwable) {
            Log.error("[Satronho hub] '$method' failed: ${e.message}")
            Bundle().apply { putString("error", e.message ?: e.javaClass.simpleName) }
        }
    }

    private fun status(ctx: Context): Bundle {
        NativeLibrary.reloadKeys()
        val firmware = if (NativeLibrary.isFirmwareAvailable()) NativeLibrary.firmwareVersion() else ""
        val pm = ctx.packageManager
        val info = pm.getPackageInfo(ctx.packageName, 0)
        return Bundle().apply {
            putBoolean("claves", NativeLibrary.areKeysPresent())
            putString("firmware", firmware)
            putBoolean("lanzador", launcherShown(ctx))
            putString("version", info.versionName ?: "")
            putLong("codigo", info.longVersionCode)
        }
    }

    private fun info(uri: String): Bundle {
        NativeLibrary.reloadKeys()
        val out = Bundle()
        if (!GameMetadata.getIsValid(uri)) {
            out.putBoolean("valido", false)
            return out
        }
        val programId = GameMetadata.getProgramId(uri)
        out.putBoolean("valido", true)
        out.putString("titulo", GameMetadata.getTitle(uri))
        out.putString("programa", programId)
        out.putString("version", GameMetadata.getVersion(uri, false))
        out.putBoolean("requiere_firmware",
            programId.isNotEmpty() && NativeLibrary.gameRequiresFirmware(programId) && !NativeLibrary.isFirmwareAvailable())
        val icon = GameMetadata.getIcon(uri)
        if (icon.isNotEmpty()) {
            // Small PNG so it fits a binder call: the hub uses it as the cover.
            val bmp = BitmapFactory.decodeByteArray(icon, 0, icon.size)
            if (bmp != null) {
                val small = Bitmap.createScaledBitmap(bmp, 256, 256, true)
                val png = ByteArrayOutputStream()
                small.compress(Bitmap.CompressFormat.PNG, 100, png)
                out.putByteArray("icono", png.toByteArray())
            }
        }
        return out
    }

    private fun keys(uri: String): Bundle {
        val code = NativeLibrary.installKeys(uri, "keys")
        NativeLibrary.reloadKeys()
        return Bundle().apply {
            putInt("resultado", code)
            putBoolean("claves", NativeLibrary.areKeysPresent())
        }
    }

    /** Same as Installable » Firmware, without dialogs: a .zip with only the .nca files. */
    private fun firmware(ctx: Context, uri: String): Bundle {
        val filterNca = FilenameFilter { _, name -> name.endsWith(".nca") }
        val firmwarePath = File(NativeConfig.getNandDir() + "/system/Contents/registered/")
        val cache = File("${ctx.cacheDir.path}/registered-hub/")
        val out = Bundle()
        try {
            cache.deleteRecursively()
            FileUtil.unzipToInternalStorage(uri, cache)
            val all = cache.list()?.size ?: -1
            val ncas = cache.list(filterNca)?.size ?: -2
            if (all <= 0 || all != ncas) {
                out.putBoolean("ok", false)
                out.putString("error", "not_firmware")
            } else {
                firmwarePath.deleteRecursively()
                cache.copyRecursively(firmwarePath, overwrite = true)
                NativeLibrary.initializeSystem(true)
                out.putBoolean("ok", true)
                out.putString("firmware", if (NativeLibrary.isFirmwareAvailable()) NativeLibrary.firmwareVersion() else "")
            }
        } finally {
            cache.deleteRecursively()
        }
        return out
    }

    private fun launcherComponent(ctx: Context) = ComponentName(ctx.packageName, LAUNCHER_ALIAS)

    private fun launcherShown(ctx: Context): Boolean {
        val state = ctx.packageManager.getComponentEnabledSetting(launcherComponent(ctx))
        return state != PackageManager.COMPONENT_ENABLED_STATE_DISABLED
    }

    private fun launcher(ctx: Context, show: Boolean): Bundle {
        ctx.packageManager.setComponentEnabledSetting(
            launcherComponent(ctx),
            if (show) PackageManager.COMPONENT_ENABLED_STATE_DEFAULT else PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
            PackageManager.DONT_KILL_APP
        )
        return Bundle().apply { putBoolean("lanzador", show) }
    }

    override fun query(uri: Uri, projection: Array<out String>?, selection: String?, selectionArgs: Array<out String>?, sortOrder: String?): Cursor? = null
    override fun getType(uri: Uri): String? = null
    override fun insert(uri: Uri, values: ContentValues?): Uri? = null
    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int = 0
    override fun update(uri: Uri, values: ContentValues?, selection: String?, selectionArgs: Array<out String>?): Int = 0

    companion object {
        const val LAUNCHER_ALIAS = "org.yuzu.yuzu_emu.Lanzador"
        private val HUB_PACKAGES = setOf("com.ridds.satronho.emulation")
    }
}
