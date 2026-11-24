// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

// Satronho NX, Android: the NDK's libc++ has no std::atomic_ref yet, which dynarmic's exclusive
// monitor and nx2 use. Force-included on Android only (root CMakeLists.txt); where the standard
// library has it, this header adds nothing.

#pragma once

#ifdef __cplusplus
#include <atomic>
#include <version>

#ifndef __cpp_lib_atomic_ref
namespace std {

template <typename T>
class atomic_ref {
public:
    explicit atomic_ref(T& object) noexcept : pointer{&object} {}

    T load(memory_order order = memory_order_seq_cst) const noexcept {
        return __atomic_load_n(pointer, Order(order));
    }

    void store(T value, memory_order order = memory_order_seq_cst) const noexcept {
        __atomic_store_n(pointer, value, Order(order));
    }

    T exchange(T value, memory_order order = memory_order_seq_cst) const noexcept {
        return __atomic_exchange_n(pointer, value, Order(order));
    }

    bool compare_exchange_strong(T& expected, T desired,
                                 memory_order order = memory_order_seq_cst) const noexcept {
        return __atomic_compare_exchange_n(pointer, &expected, desired, false, Order(order),
                                           FailureOrder(order));
    }

    bool compare_exchange_weak(T& expected, T desired,
                               memory_order order = memory_order_seq_cst) const noexcept {
        return __atomic_compare_exchange_n(pointer, &expected, desired, true, Order(order),
                                           FailureOrder(order));
    }

private:
    static constexpr int Order(memory_order order) noexcept {
        switch (order) {
        case memory_order_relaxed:
            return __ATOMIC_RELAXED;
        case memory_order_consume:
            return __ATOMIC_CONSUME;
        case memory_order_acquire:
            return __ATOMIC_ACQUIRE;
        case memory_order_release:
            return __ATOMIC_RELEASE;
        case memory_order_acq_rel:
            return __ATOMIC_ACQ_REL;
        default:
            return __ATOMIC_SEQ_CST;
        }
    }

    // A failed compare-exchange only loads: no release part.
    static constexpr int FailureOrder(memory_order order) noexcept {
        switch (order) {
        case memory_order_release:
            return __ATOMIC_RELAXED;
        case memory_order_acq_rel:
            return __ATOMIC_ACQUIRE;
        default:
            return Order(order);
        }
    }

    T* pointer;
};

} // namespace std
#endif // __cpp_lib_atomic_ref
#endif // __cplusplus
