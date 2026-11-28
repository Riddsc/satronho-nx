// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2022 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <cstdio>

#include <algorithm>
#include <atomic>
#ifdef __ANDROID__
#include <unistd.h>
#endif
#include <bit>

#include "dynarmic/backend/arm64/a64_address_space.h"
#include "dynarmic/backend/arm64/a64_jitstate.h"
#include "dynarmic/backend/arm64/abi.h"
#include "dynarmic/backend/arm64/devirtualize.h"
#include "dynarmic/backend/arm64/emit_arm64.h"
#include "dynarmic/backend/arm64/stack_layout.h"
#include "dynarmic/common/cast_util.h"
#include "dynarmic/common/fp/fpcr.h"
#include "dynarmic/common/llvm_disassemble.h"
#include "dynarmic/interface/exclusive_monitor.h"
#include "dynarmic/ir/location_descriptor.h"

namespace Dynarmic::Backend::Arm64 {

// Satronho NX: clears forced by a full code cache (not by the block limit), for the phone's log.
std::atomic<std::uint64_t> g_code_full_clears{0};

AddressSpace::AddressSpace(std::size_t code_cache_size)
    : ir_block{IR::LocationDescriptor{0}}
    , code_cache_size(code_cache_size)
    , mem(code_cache_size)
    , code(mem.ptr(), mem.ptr())
    , fastmem_manager(exception_handler)
{
    ASSERT(code_cache_size <= 128 * 1024 * 1024 && "code_cache_size > 128 MiB not currently supported");

    exception_handler.Register(mem, code_cache_size);
    exception_handler.SetFastmemCallback([this](u64 host_pc) {
        return FastmemCallback(host_pc);
    });
}

AddressSpace::~AddressSpace() = default;

CodePtr AddressSpace::Get(IR::LocationDescriptor descriptor) {
    if (const auto iter = block_entries.find(descriptor); iter != block_entries.end()) {
        return iter->second;
    }
    return nullptr;
}

std::optional<IR::LocationDescriptor> AddressSpace::ReverseGetLocation(CodePtr host_pc) {
    if (auto iter = reverse_block_entries.upper_bound(host_pc); iter != reverse_block_entries.begin()) {
        // upper_bound locates the first value greater than host_pc, so we need to decrement
        --iter;
        return iter->second;
    }
    return std::nullopt;
}

CodePtr AddressSpace::ReverseGetEntryPoint(CodePtr host_pc) {
    if (auto iter = reverse_block_entries.upper_bound(host_pc); iter != reverse_block_entries.begin()) {
        // upper_bound locates the first value greater than host_pc, so we need to decrement
        --iter;
        return iter->first;
    }
    return nullptr;
}

CodePtr AddressSpace::GetOrEmit(IR::LocationDescriptor descriptor) {
    if (CodePtr block_entry = Get(descriptor)) {
        return block_entry;
    }
    GenerateIR(ir_block, descriptor);
    return Emit(std::move(ir_block));
}

void AddressSpace::InvalidateBasicBlocks(const ::Common::unordered_set<IR::LocationDescriptor>& descriptors) {
    UnprotectCodeMemory();

    for (const auto& descriptor : descriptors) {
        const auto iter = block_entries.find(descriptor);
        if (iter == block_entries.end()) {
            continue;
        }

        // Unlink before removal because InvalidateBasicBlocks can be called within a fastmem callback,
        // and the currently executing block may have references to itself which need to be unlinked.
        RelinkForDescriptor(descriptor, nullptr);

#ifdef __ANDROID__
        // Its tables may still be needed until this run ends (a fastmem fault in the block that
        // is executing), so they are freed at the start of the next one.
        invalidated_blocks.push_back(iter->second);
#endif
        block_entries.erase(iter);
    }

    ProtectCodeMemory();
}

void AddressSpace::ClearCache() {
    block_entries.clear();
    reverse_block_entries.clear();
    block_infos.clear();
    block_references.clear();
    code.set_offset(prelude_info.end_of_prelude);
    OnCacheCleared();
#ifdef __ANDROID__
    full_clear_requested = false;
    invalidated_blocks.clear();
#endif
}

#ifdef __ANDROID__
void AddressSpace::FreeInvalidatedBlocks() {
    for (const CodePtr entry_point : invalidated_blocks) {
        const auto info = block_infos.find(entry_point);
        if (info == block_infos.end()) {
            continue;
        }
        for (const auto& link : info->second.block_relocations) {
            if (const auto refs = block_references.find(link.target); refs != block_references.end()) {
                refs->second.erase(entry_point);
                if (refs->second.empty()) {
                    block_references.erase(refs);
                }
            }
        }
        block_infos.erase(info);
        reverse_block_entries.erase(entry_point);
    }
    invalidated_blocks.clear();
}
#endif

std::size_t AddressSpace::GetRemainingSize() {
    return code_cache_size - static_cast<std::size_t>(code.offset());
}

CodePtr AddressSpace::Emit(IR::Block block) {
    if (GetRemainingSize() < 1024 * 1024) {
        g_code_full_clears.fetch_add(1, std::memory_order_relaxed);
        ClearCache();
    }
#ifdef __ANDROID__
    // Satronho NX: each block keeps link and fastmem tables beside its code, so on a phone those
    // tables reached GBs (Ocarina, 6 cores) long before the 128 MiB of code filled and the
    // emulator ran out of memory. Start over once a core holds this many blocks; if the last
    // start over was less than 30 s ago the game's code does not fit, so keep more blocks first
    // (up to 4x) instead of translating the same code again and again. Clearing here, while code
    // runs, would leave the return stack buffer pointing into reused code, so the JIT is asked to
    // clear at the start of its next run instead.
    else if (block_infos.size() >= full_clear_blocks && !full_clear_requested && request_full_clear) {
        // Satronho NX: 196608 (was 131072) on phones with more than 9 GiB: Ocarina started over
        // 1-5 times every 12 s in Kokiri Forest, and ~60 % less often with this. The tables cost
        // memory (malloc reached 3.2 GiB on a Pixel 6 Pro), so phones with 9 GiB or less (one
        // with 8 GB was closed by Android for low memory) stop at 65536.
        static const std::size_t max_full_clear_blocks = [] {
            const long pages = sysconf(_SC_PHYS_PAGES);
            const long page_size = sysconf(_SC_PAGE_SIZE);
            const bool small = pages > 0 && page_size > 0 &&
                               static_cast<std::uint64_t>(pages) *
                                       static_cast<std::uint64_t>(page_size) <=
                                   (std::uint64_t{9} << 30);
            return small ? std::size_t{65536} : std::size_t{196608};
        }();
        const auto now = std::chrono::steady_clock::now();
        if (now - last_full_clear < std::chrono::seconds{30} && full_clear_blocks < max_full_clear_blocks) {
            full_clear_blocks *= 2;
        } else {
            last_full_clear = now;
            full_clear_requested = true;
            request_full_clear();
        }
    }
#endif

    UnprotectCodeMemory();

    EmittedBlockInfo block_info = EmitArm64(code, std::move(block), GetEmitConfig(), fastmem_manager);
    const CodePtr entry_point = block_info.entry_point;
    const std::size_t size = block_info.size;

    ASSERT(block_entries.insert({block.Location(), entry_point}).second);
    ASSERT(reverse_block_entries.insert({entry_point, block.Location()}).second);

    Link(block_info);
    // The relocations are only needed to link this block once; keeping them cost memory per block.
    block_info.relocations = {};
    if (!std::is_sorted(block_info.fastmem_patch_info.begin(), block_info.fastmem_patch_info.end(),
                        [](const auto& a, const auto& b) { return a.code_offset < b.code_offset; })) {
        std::sort(block_info.fastmem_patch_info.begin(), block_info.fastmem_patch_info.end(),
                  [](const auto& a, const auto& b) { return a.code_offset < b.code_offset; });
    }
    block_info.fastmem_patch_info.shrink_to_fit();
    block_info.block_relocations.shrink_to_fit();
    const auto [stored, inserted] = block_infos.emplace(entry_point, std::move(block_info));
    ASSERT(inserted);

    RelinkForDescriptor(block.Location(), entry_point);

    mem.invalidate(reinterpret_cast<u32*>(entry_point), size);
    ProtectCodeMemory();

    RegisterNewBasicBlock(block, stored->second);

    return entry_point;
}

void AddressSpace::Link(EmittedBlockInfo& block_info) {
    using namespace oaknut;
    using namespace oaknut::util;

    for (auto [ptr_offset, target] : block_info.relocations) {
        CodeGenerator c{mem.ptr(), mem.ptr()};
        c.set_xptr(reinterpret_cast<u32*>(block_info.entry_point + ptr_offset));

        switch (target) {
        case LinkTarget::ReturnToDispatcher:
            c.B(prelude_info.return_to_dispatcher);
            break;
        case LinkTarget::ReturnFromRunCode:
            c.B(prelude_info.return_from_run_code);
            break;
        // { this, vaddr, size }
        case LinkTarget::ReadMemory8:
            c.BL(prelude_info.read_memory_8);
            break;
        case LinkTarget::ReadMemory16:
            c.BL(prelude_info.read_memory_16);
            break;
        case LinkTarget::ReadMemory32:
            c.BL(prelude_info.read_memory_32);
            break;
        case LinkTarget::ReadMemory64:
            c.BL(prelude_info.read_memory_64);
            break;
        case LinkTarget::ReadMemory128:
            c.BL(prelude_info.read_memory_128);
            break;
        // { this, vaddr, size }
        case LinkTarget::WrappedReadMemory8:
            c.BL(prelude_info.wrapped_read_memory_8);
            break;
        case LinkTarget::WrappedReadMemory16:
            c.BL(prelude_info.wrapped_read_memory_16);
            break;
        case LinkTarget::WrappedReadMemory32:
            c.BL(prelude_info.wrapped_read_memory_32);
            break;
        case LinkTarget::WrappedReadMemory64:
            c.BL(prelude_info.wrapped_read_memory_64);
            break;
        case LinkTarget::WrappedReadMemory128:
            c.BL(prelude_info.wrapped_read_memory_128);
            break;
        //
        case LinkTarget::ExclusiveReadMemory8:
            c.BL(prelude_info.exclusive_read_memory_8);
            break;
        case LinkTarget::ExclusiveReadMemory16:
            c.BL(prelude_info.exclusive_read_memory_16);
            break;
        case LinkTarget::ExclusiveReadMemory32:
            c.BL(prelude_info.exclusive_read_memory_32);
            break;
        case LinkTarget::ExclusiveReadMemory64:
            c.BL(prelude_info.exclusive_read_memory_64);
            break;
        case LinkTarget::ExclusiveReadMemory128:
            c.BL(prelude_info.exclusive_read_memory_128);
            break;
        // { this, vaddr, value, size }
        case LinkTarget::WriteMemory8:
            c.BL(prelude_info.write_memory_8);
            break;
        case LinkTarget::WriteMemory16:
            c.BL(prelude_info.write_memory_16);
            break;
        case LinkTarget::WriteMemory32:
            c.BL(prelude_info.write_memory_32);
            break;
        case LinkTarget::WriteMemory64:
            c.BL(prelude_info.write_memory_64);
            break;
        case LinkTarget::WriteMemory128:
            c.BL(prelude_info.write_memory_128);
            break;
        //
        case LinkTarget::WrappedWriteMemory8:
            c.BL(prelude_info.wrapped_write_memory_8);
            break;
        case LinkTarget::WrappedWriteMemory16:
            c.BL(prelude_info.wrapped_write_memory_16);
            break;
        case LinkTarget::WrappedWriteMemory32:
            c.BL(prelude_info.wrapped_write_memory_32);
            break;
        case LinkTarget::WrappedWriteMemory64:
            c.BL(prelude_info.wrapped_write_memory_64);
            break;
        case LinkTarget::WrappedWriteMemory128:
            c.BL(prelude_info.wrapped_write_memory_128);
            break;
        //
        case LinkTarget::ExclusiveWriteMemory8:
            c.BL(prelude_info.exclusive_write_memory_8);
            break;
        case LinkTarget::ExclusiveWriteMemory16:
            c.BL(prelude_info.exclusive_write_memory_16);
            break;
        case LinkTarget::ExclusiveWriteMemory32:
            c.BL(prelude_info.exclusive_write_memory_32);
            break;
        case LinkTarget::ExclusiveWriteMemory64:
            c.BL(prelude_info.exclusive_write_memory_64);
            break;
        case LinkTarget::ExclusiveWriteMemory128:
            c.BL(prelude_info.exclusive_write_memory_128);
            break;
        case LinkTarget::CallSVC:
            c.BL(prelude_info.call_svc);
            break;
        case LinkTarget::ExceptionRaised:
            c.BL(prelude_info.exception_raised);
            break;
        case LinkTarget::InstructionSynchronizationBarrierRaised:
            c.BL(prelude_info.isb_raised);
            break;
        case LinkTarget::InstructionCacheOperationRaised:
            c.BL(prelude_info.ic_raised);
            break;
        case LinkTarget::DataCacheOperationRaised:
            c.BL(prelude_info.dc_raised);
            break;
        case LinkTarget::GetCNTPCT:
            c.BL(prelude_info.get_cntpct);
            break;
        case LinkTarget::AddTicks:
            c.BL(prelude_info.add_ticks);
            break;
        case LinkTarget::GetTicksRemaining:
            c.BL(prelude_info.get_ticks_remaining);
            break;
        default:
            UNREACHABLE();
        }
    }

    for (const auto& link : block_info.block_relocations) {
        block_references[link.target].insert(block_info.entry_point);
        LinkBlockLink(block_info.entry_point, Get(link.target), link.relocation);
    }
}

void AddressSpace::LinkBlockLink(const CodePtr entry_point, const CodePtr target_ptr, const BlockRelocation& block_relocation) {
    using namespace oaknut;
    using namespace oaknut::util;

    {
        const auto [ptr_offset, type] = block_relocation;
        CodeGenerator c{mem.ptr(), mem.ptr()};
        c.set_xptr(reinterpret_cast<u32*>(entry_point + ptr_offset));

        switch (type) {
        case BlockRelocationType::Branch:
            if (target_ptr) {
                c.B((void*)target_ptr);
            } else {
                c.NOP();
            }
            break;
        case BlockRelocationType::MoveToScratch1:
            if (target_ptr) {
                c.ADRL(Xscratch1, (void*)target_ptr);
            } else {
                c.ADRL(Xscratch1, prelude_info.return_to_dispatcher);
            }
            break;
        default:
            UNREACHABLE();
        }
    }
}

void AddressSpace::RelinkForDescriptor(IR::LocationDescriptor target_descriptor, CodePtr target_ptr) {
    for (auto code_ptr : block_references[target_descriptor]) {
        if (auto block_iter = block_infos.find(code_ptr); block_iter != block_infos.end()) {
            const EmittedBlockInfo& block_info = block_iter->second;

            for (const auto& link : block_info.block_relocations) {
                if (link.target == target_descriptor) {
                    LinkBlockLink(block_info.entry_point, target_ptr, link.relocation);
                }
            }

            mem.invalidate(reinterpret_cast<u32*>(block_info.entry_point), block_info.size);
        }
    }
}

FakeCall AddressSpace::FastmemCallback(u64 host_pc) {
    {
        const auto host_ptr = std::bit_cast<CodePtr>(host_pc);

        const auto entry_point = ReverseGetEntryPoint(host_ptr);
        if (!entry_point) {
            goto fail;
        }

        const auto block_info = block_infos.find(entry_point);
        if (block_info == block_infos.end()) {
            goto fail;
        }

        const auto& patches = block_info->second.fastmem_patch_info;
        const std::ptrdiff_t offset = host_ptr - entry_point;
        const auto patch_entry = std::lower_bound(patches.begin(), patches.end(), offset,
                                                  [](const FastmemPatchEntry& entry, std::ptrdiff_t value) { return entry.code_offset < value; });
        if (patch_entry == patches.end() || patch_entry->code_offset != offset) {
            goto fail;
        }

        const auto fc = patch_entry->info.fc;

        if (patch_entry->info.recompile) {
            const auto marker = patch_entry->info.marker;
            fastmem_manager.MarkDoNotFastmem(marker);
            InvalidateBasicBlocks({std::get<0>(marker)});
        }

        return fc;
    }

fail:
    fmt::print("dynarmic: Segfault happened within JITted code at host_pc = {:016x}\n"
        "Segfault wasn't at a fastmem patch location!\n", host_pc);
    UNREACHABLE();
}

}  // namespace Dynarmic::Backend::Arm64
