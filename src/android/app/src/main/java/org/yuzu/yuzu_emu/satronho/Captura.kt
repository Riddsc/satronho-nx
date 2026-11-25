// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import android.content.ContentValues
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.Typeface
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.provider.MediaStore
import android.view.PixelCopy
import android.view.SurfaceView
import android.widget.Toast
import org.yuzu.yuzu_emu.R
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * «Captura Satronho»: the game's picture with a discreet band at the bottom (our planet, Satronho
 * NX, the game and its FPS), saved in Pictures/Satronho NX and ready to share.
 */
object Captura {
    fun take(activity: Activity, surface: SurfaceView, gameTitle: String, fps: Double) {
        val width = surface.width
        val height = surface.height
        if (width <= 0 || height <= 0 || surface.holder.surface?.isValid != true) {
            Toast.makeText(activity, R.string.satronho_capture_failed, Toast.LENGTH_SHORT).show()
            return
        }
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val thread = HandlerThread("SatronhoCaptura").apply { start() }
        PixelCopy.request(surface, bitmap, { result ->
            thread.quitSafely()
            Handler(Looper.getMainLooper()).post {
                if (result != PixelCopy.SUCCESS) {
                    Toast.makeText(activity, R.string.satronho_capture_failed, Toast.LENGTH_SHORT).show()
                    return@post
                }
                save(activity, stamp(activity, bitmap, gameTitle, fps))
            }
        }, Handler(thread.looper))
    }

    private fun stamp(activity: Activity, source: Bitmap, gameTitle: String, fps: Double): Bitmap {
        val out = source.copy(Bitmap.Config.ARGB_8888, true)
        val canvas = Canvas(out)
        val band = (out.height * 0.11f).coerceAtLeast(56f)
        val top = out.height - band
        canvas.drawRect(
            RectF(0f, top - band * 0.6f, out.width.toFloat(), out.height.toFloat()),
            Paint().apply {
                shader = LinearGradient(
                    0f, top - band * 0.6f, 0f, out.height.toFloat(),
                    0x000F0D16, 0xE60F0D16.toInt(), Shader.TileMode.CLAMP
                )
            }
        )
        val margin = band * 0.25f
        val logoSize = (band * 0.7f).toInt()
        try {
            activity.assets.open("logo_fondo.png").use { stream ->
                BitmapFactory.decodeStream(stream)?.let { logo ->
                    val left = margin.toInt()
                    val logoTop = (top + (band - logoSize) / 2).toInt()
                    canvas.drawBitmap(
                        logo, null, Rect(left, logoTop, left + logoSize, logoTop + logoSize),
                        Paint(Paint.FILTER_BITMAP_FLAG)
                    )
                }
            }
        } catch (e: Exception) {
            // The band still carries the name.
        }
        val textLeft = margin * 2 + logoSize
        val name = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Satronho.LILAC
            textSize = band * 0.3f
            typeface = Typeface.DEFAULT_BOLD
        }
        val detail = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFF2EEFF.toInt()
            textSize = band * 0.24f
        }
        canvas.drawText("Satronho NX", textLeft, top + band * 0.45f, name)
        val line = if (fps > 0.5) String.format("%s · %.0f FPS", gameTitle, fps) else gameTitle
        canvas.drawText(line, textLeft, top + band * 0.8f, detail)
        return out
    }

    private fun save(activity: Activity, bitmap: Bitmap) {
        val name = "Satronho_" + SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(Date()) + ".png"
        val values = ContentValues().apply {
            put(MediaStore.Images.Media.DISPLAY_NAME, name)
            put(MediaStore.Images.Media.MIME_TYPE, "image/png")
            put(MediaStore.Images.Media.RELATIVE_PATH, "Pictures/Satronho NX")
        }
        val resolver = activity.contentResolver
        val uri = try {
            resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values)?.also { target ->
                resolver.openOutputStream(target)?.use { bitmap.compress(Bitmap.CompressFormat.PNG, 100, it) }
            }
        } catch (e: Exception) {
            null
        }
        if (uri == null) {
            Toast.makeText(activity, R.string.satronho_capture_failed, Toast.LENGTH_SHORT).show()
            return
        }
        Toast.makeText(activity, R.string.satronho_capture_saved, Toast.LENGTH_SHORT).show()
        val intent = Intent(Intent.ACTION_SEND)
            .setType("image/png")
            .putExtra(Intent.EXTRA_STREAM, uri)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        activity.startActivity(Intent.createChooser(intent, activity.getString(R.string.satronho_capture)))
    }
}
