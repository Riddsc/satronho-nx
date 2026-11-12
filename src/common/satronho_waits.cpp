// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/satronho_waits.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace Satronho::Waits {

namespace {

std::array<std::atomic<u64>, static_cast<size_t>(Kind::Count)> g_ns{};
std::array<std::atomic<u64>, static_cast<size_t>(Kind::Count)> g_count{};

struct Sum {
    u64 ns{};
    u64 count{};
};

std::mutex g_labels_mutex;
std::unordered_map<std::string, Sum> g_labels;

std::mutex g_ports_mutex;
std::unordered_map<const void*, std::string> g_ports;

} // namespace

void Add(Kind kind, std::string_view label, u64 ns, bool always) {
    if (!always && ns >= ShortWaitNs) {
        return;
    }
    const auto index = static_cast<size_t>(kind);
    g_ns[index].fetch_add(ns, std::memory_order_relaxed);
    g_count[index].fetch_add(1, std::memory_order_relaxed);
    if (label.empty()) {
        return;
    }
    std::string key{kind_names[index]};
    key += ':';
    key += label;
    std::scoped_lock lock{g_labels_mutex};
    Sum& sum = g_labels[key];
    sum.ns += ns;
    ++sum.count;
}

void AddLabel(Kind kind, std::string_view label, u64 ns) {
    std::string key{kind_names[static_cast<size_t>(kind)]};
    key += ':';
    key += label;
    std::scoped_lock lock{g_labels_mutex};
    Sum& sum = g_labels[key];
    sum.ns += ns;
    ++sum.count;
}

void NamePort(const void* port, std::string name) {
    std::scoped_lock lock{g_ports_mutex};
    g_ports[port] = std::move(name);
}

std::string PortName(const void* port) {
    std::scoped_lock lock{g_ports_mutex};
    const auto it = g_ports.find(port);
    return it != g_ports.end() ? it->second : std::string{"?"};
}

Totals ReadTotals() {
    Totals totals;
    for (size_t i = 0; i < totals.ns.size(); ++i) {
        totals.ns[i] = g_ns[i].load(std::memory_order_relaxed);
        totals.count[i] = g_count[i].load(std::memory_order_relaxed);
    }
    return totals;
}

std::vector<Labeled> ReadLabels() {
    std::scoped_lock lock{g_labels_mutex};
    std::vector<Labeled> labels;
    labels.reserve(g_labels.size());
    for (const auto& [label, sum] : g_labels) {
        labels.push_back({label, sum.ns, sum.count});
    }
    return labels;
}

} // namespace Satronho::Waits
