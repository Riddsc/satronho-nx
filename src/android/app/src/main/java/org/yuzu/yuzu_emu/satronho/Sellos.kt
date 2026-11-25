// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import android.widget.TextView
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.model.Game

/**
 * Satronho's mark on each game's card: ✓ when the game is prepared, and how its last session went
 * («60 fps»). The list view also gets the whole line under the developer.
 */
object Sellos {
    fun bind(badge: TextView?, game: Game, details: TextView? = null, developer: String = "") {
        val context = (badge ?: details)?.context ?: return
        val prepared = PrepareGame.isPrepared(context, game.programId)
        val last = SesionActual.last(context, game.programId)

        badge?.let { view ->
            val parts = ArrayList<String>()
            if (prepared) {
                parts.add("✓")
            }
            if (last != null) {
                parts.add(String.format("%.0f fps", last.averageFps))
            }
            if (parts.isEmpty()) {
                view.visibility = android.view.View.GONE
            } else {
                view.text = parts.joinToString(" ")
                Satronho.styleTag(view, if (prepared) Satronho.GREEN else Satronho.LILAC)
                view.contentDescription = listOfNotNull(
                    if (prepared) context.getString(R.string.satronho_card_prepared) else null,
                    last?.let { ResumenSesion.cardLine(context, it) }
                ).joinToString(", ")
                view.visibility = android.view.View.VISIBLE
            }
        }

        details?.let { view ->
            val parts = ArrayList<String>()
            if (developer.isNotEmpty()) {
                parts.add(developer)
            }
            if (prepared) {
                parts.add(context.getString(R.string.satronho_card_prepared))
            }
            last?.let { parts.add(ResumenSesion.cardLine(context, it)) }
            view.text = parts.joinToString(" · ")
        }
    }
}
