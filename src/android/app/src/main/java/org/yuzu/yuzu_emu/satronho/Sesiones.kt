// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import android.content.Intent
import android.provider.DocumentsContract
import android.widget.Toast
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import java.io.File
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.DocumentProvider
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.FileUtil

/**
 * Satronho NX's session history: every finished session's log is kept in log/sesiones, named by
 * when it ended, its build and its games (common/logging.cpp). Pick one to share it.
 */
object Sesiones {
    private fun folder(): File? =
        DirectoryInitialization.userDirectory?.let { File(it, "log/sesiones") }

    fun files(): List<File> =
        folder()?.listFiles { file -> file.isFile && file.name.endsWith(".txt") }
            ?.sortedByDescending { it.lastModified() }
            ?: emptyList()

    /** "2026-11-24_08-05-12_build-56_0400486024DEC000.txt" -> "2026-11-24 08:05 · build-56". */
    private fun label(file: File): String {
        val parts = file.nameWithoutExtension.split('_')
        val date = parts.getOrNull(0) ?: return file.name
        val time = parts.getOrNull(1)?.replace('-', ':')?.take(5) ?: ""
        val rest = parts.drop(2).joinToString(" · ")
        return listOf("$date $time".trim(), rest).filter { it.isNotEmpty() }.joinToString(" · ")
    }

    fun share(activity: Activity, file: File) {
        val base = DirectoryInitialization.userDirectory ?: return
        val relative = file.relativeTo(File(base)).path
        val uri = DocumentsContract.buildDocumentUri(
            DocumentProvider.AUTHORITY,
            "${DocumentProvider.ROOT_ID}/$relative"
        )
        val intent = Intent(Intent.ACTION_SEND)
            .setDataAndType(uri, FileUtil.TEXT_PLAIN)
            .putExtra(Intent.EXTRA_STREAM, uri)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        activity.startActivity(
            Intent.createChooser(intent, activity.getString(R.string.satronho_sessions_share))
        )
    }

    fun show(activity: Activity) {
        val list = files()
        if (list.isEmpty()) {
            Toast.makeText(activity, R.string.satronho_sessions_empty, Toast.LENGTH_LONG).show()
            return
        }
        val labels = list.map { label(it) }.toTypedArray()
        MaterialAlertDialogBuilder(activity)
            .setTitle(R.string.satronho_sessions)
            .setItems(labels) { _, which -> share(activity, list[which]) }
            .setNegativeButton(R.string.satronho_close, null)
            .show()
    }
}
