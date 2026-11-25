// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.app.Activity
import android.content.Context
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.materialswitch.MaterialSwitch
import com.google.android.material.tabs.TabLayout
import org.yuzu.yuzu_emu.R

/**
 * The Satronho screen, as the desktop's Satronho window: Novedades, Configuraciones sugeridas,
 * Pregúntale a los guías and Guías, in tabs. Opened from the settings, the guides' bubble (long
 * press) and the launcher icon's shortcut.
 */
object SatronhoScreen {
    const val TAB_NEWS = 0
    const val TAB_SUGGESTED = 1
    const val TAB_ASK = 2
    const val TAB_GUIDES = 3
    const val TAB_STYLE = 4

    fun show(activity: Activity, onClose: (() -> Unit)? = null, tab: Int = -1) {
        val context: Context = activity
        val root = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }

        // Header: our logo, the name and what this build is.
        val header = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            val pad = Satronho.dp(context, 20)
            setPadding(pad, Satronho.dp(context, 18), pad, Satronho.dp(context, 6))
        }
        val logoSize = Satronho.dp(context, 44)
        header.addView(ImageView(context).apply {
            layoutParams = LinearLayout.LayoutParams(logoSize, logoSize)
            setImageResource(R.drawable.ic_yuzu)
        })
        header.addView(LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(Satronho.dp(context, 12), 0, 0, 0)
            addView(TextView(context).apply {
                text = context.getString(R.string.app_name)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 20f)
                typeface = Typeface.DEFAULT_BOLD
                setTextColor(Satronho.LILAC)
            })
            addView(TextView(context).apply {
                text = buildLabel(context)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
                setTextColor(Satronho.DIM)
            })
        })
        root.addView(header)

        val tabs = TabLayout(context).apply {
            tabMode = TabLayout.MODE_SCROLLABLE
            setSelectedTabIndicatorColor(Satronho.VIOLET)
            setTabTextColors(Satronho.DIM, Satronho.LILAC)
        }
        listOf(
            R.string.satronho_novedades,
            R.string.satronho_suggested,
            R.string.satronho_ask_tab,
            R.string.satronho_guides_tab,
            R.string.satronho_style_tab
        ).forEach { tabs.addTab(tabs.newTab().setText(it)) }
        root.addView(tabs)

        val height = (context.resources.displayMetrics.heightPixels * 0.58f).toInt()
        val body = FrameLayout(context)
        root.addView(body, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, height))

        val dialogBuilder = MaterialAlertDialogBuilder(context)
            .setView(root)
            .setPositiveButton(R.string.satronho_close, null)
            .setNeutralButton(R.string.satronho_share_log) { _, _ -> Satronho.shareLatestLog(activity) }
            .setOnDismissListener {
                Novedades.markSeen(context)
                onClose?.invoke()
            }

        lateinit var showTabRef: (Int) -> Unit
        fun showTab(index: Int) {
            body.removeAllViews()
            val view = when (index) {
                TAB_NEWS -> Novedades.buildView(context)
                TAB_SUGGESTED -> suggestedTab(context)
                TAB_ASK -> GuideBubble.buildChat(activity, Satronho.speaker("samy"))
                TAB_STYLE -> Estilo.buildView(activity) { showTabRef(TAB_STYLE) }
                else -> guidesTab(activity)
            }
            body.addView(view, FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            ))
        }
        showTabRef = ::showTab
        tabs.addOnTabSelectedListener(object : TabLayout.OnTabSelectedListener {
            override fun onTabSelected(tab: TabLayout.Tab) = showTab(tab.position)
            override fun onTabUnselected(tab: TabLayout.Tab) {}
            override fun onTabReselected(tab: TabLayout.Tab) {}
        })

        // News first when there is something new, as on the desktop; otherwise ask the guides.
        val first = when {
            tab >= 0 -> tab
            Novedades.hasUnseen(context) -> TAB_NEWS
            else -> TAB_ASK
        }
        tabs.getTabAt(first)?.select()
        showTab(first)
        dialogBuilder.show()
    }

    private fun buildLabel(context: Context): String {
        val version = try {
            org.yuzu.yuzu_emu.NativeLibrary.getBuildVersion()
        } catch (e: Exception) {
            ""
        }
        return if (version.isEmpty()) {
            context.getString(R.string.satronho_screen_subtitle)
        } else {
            "${context.getString(R.string.satronho_screen_subtitle)} · $version"
        }
    }

    private fun suggestedTab(context: Context): View {
        val content = Sugeridas.buildContent(context)
        return LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            addView(content.view, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f
            ))
            addView(Button(context).apply {
                text = context.getString(R.string.satronho_apply)
                setOnClickListener { content.apply() }
            }, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                gravity = Gravity.END
                marginEnd = Satronho.dp(context, 20)
            })
        }
    }

    /** The five guides, with their color, size and topic; each pet can be turned off. */
    private fun guidesTab(activity: Activity): View {
        val context: Context = activity
        val pad = Satronho.dp(context, 20)
        val list = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, Satronho.dp(context, 8), pad, Satronho.dp(context, 8))
        }
        list.addView(TextView(context).apply {
            text = context.getString(R.string.satronho_guides_intro)
            setTextColor(Satronho.DIM)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            setPadding(0, 0, 0, Satronho.dp(context, 8))
        })
        Satronho.GUIDES.forEach { guide ->
            val row = LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                setPadding(0, Satronho.dp(context, 10), 0, Satronho.dp(context, 10))
            }
            // Each avatar keeps the guide's real size, inside the same 68 dp column.
            val column = Satronho.dp(context, 68)
            val frame = FrameLayout(context)
            val size = Satronho.dp(context, guide.sizeDp)
            val ring = Satronho.dp(context, 3)
            frame.addView(ImageView(context).apply {
                setPadding(ring, ring, ring, ring)
                background = GradientDrawable().apply {
                    shape = GradientDrawable.OVAL
                    setColor(0xFF1A1626.toInt())
                    setStroke(ring, guide.color)
                }
                Satronho.guideBitmap(context, guide.key)?.let { bitmap ->
                    setImageDrawable(
                        RoundedBitmapDrawableFactory.create(resources, bitmap).apply { isCircular = true }
                    )
                }
                contentDescription = guide.name
            }, FrameLayout.LayoutParams(size, size, Gravity.CENTER))
            row.addView(frame, LinearLayout.LayoutParams(column, column))

            val text = LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(Satronho.dp(context, 12), 0, Satronho.dp(context, 8), 0)
            }
            text.addView(TextView(context).apply {
                this.text = guide.name
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 17f)
                typeface = Typeface.DEFAULT_BOLD
                setTextColor(guide.color)
            })
            text.addView(TextView(context).apply {
                this.text = context.getString(guide.roleId)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
                typeface = Typeface.DEFAULT_BOLD
            })
            text.addView(TextView(context).apply {
                this.text = context.getString(guide.aboutId)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
                setTextColor(Satronho.DIM)
            })
            row.addView(text, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))

            if (guide.key != "mecharidds") {
                row.addView(MaterialSwitch(context).apply {
                    isChecked = Satronho.petEnabled(guide.key)
                    contentDescription = guide.name
                    setOnCheckedChangeListener { _, checked ->
                        Satronho.setPetEnabled(guide.key, checked)
                    }
                })
            }
            list.addView(row)
        }
        return ScrollView(context).apply { addView(list) }
    }
}
