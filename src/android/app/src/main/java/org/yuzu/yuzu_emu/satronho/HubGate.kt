// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.satronho

import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.locks.ReentrantLock

/**
 * Turno para usar el núcleo de NX entre el hub y NX.
 *
 * El hub le pregunta cosas a NX por el HubProvider en otro hilo (estado, nombre e ícono de un
 * juego, claves...). Varias de esas preguntas recargan las claves o rehacen el sistema de archivos
 * del núcleo. Si eso pasaba justo mientras NX revisaba el juego que el hub le mandó abrir, el
 * juego parecía inválido y salía «No bootable game present!». Con este turno:
 *  - el HubProvider espera a que NX termine de arrancar (Android le deja recibir preguntas
 *    antes de que la aplicación acabe su onCreate);
 *  - el hub y la revisión del juego nunca usan el núcleo al mismo tiempo;
 *  - mientras hay un juego abierto, NX no atiende al hub (solo le dice que está ocupado).
 */
object HubGate {
    /** Intent extra: the update/DLC files (URIs) the hub sends with a game. */
    const val EXTRA_FILES = "satronho_files"

    private val ready = CountDownLatch(1)
    private val lock = ReentrantLock()

    /** Hay una pantalla de juego abierta (desde que se crea hasta que se cierra). */
    @Volatile
    var playing = false

    /** Las claves ya se cargaron en este proceso: no hace falta recargarlas en cada pregunta. */
    @Volatile
    var keysLoaded = false

    /** YuzuApplication terminó su onCreate (carpetas, configuración y núcleo listos). */
    fun markReady() = ready.countDown()

    fun awaitReady(timeoutMs: Long = 20_000): Boolean =
        ready.await(timeoutMs, TimeUnit.MILLISECONDS)

    /** Hace [block] con el núcleo para sí solo. Si no consigue el turno a tiempo devuelve null. */
    fun <T> withCore(timeoutMs: Long = 10_000, block: () -> T): T? {
        if (!lock.tryLock(timeoutMs, TimeUnit.MILLISECONDS)) return null
        try {
            return block()
        } finally {
            lock.unlock()
        }
    }
}
