// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <utility>
#include <vector>

#include "qt_common/qt_common.h"

#include "common/dynamic_library.h"
#include "common/logging.h"
#include "qt_common/util/vk.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_library.h"
#include "video_core/vulkan_common/vulkan_surface.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"
#include "vulkan/vulkan_core.h"

class QWindow;

namespace VkDeviceInfo {
Record::Record(std::string_view name_, const std::vector<VkPresentModeKHR>& vsync_modes_,
               bool has_broken_compute_, std::string_view device_name_, u32 vendor_id_,
               u64 device_local_bytes_)
    : name{name_}, vsync_support{vsync_modes_}, has_broken_compute{has_broken_compute_},
      device_name{device_name_}, vendor_id{vendor_id_}, device_local_bytes{device_local_bytes_} {}

Record::~Record() = default;

void PopulateRecords(std::vector<Record>& records, QWindow* window) try {
    using namespace Vulkan;

    // Create a test window with a Vulkan surface type for checking present modes.
    QWindow test_window(window);
    test_window.setSurfaceType(QWindow::VulkanSurface);
    test_window.create();
    auto wsi = QtCommon::GetWindowSystemInfo(&test_window);

    vk::InstanceDispatch dld;
    const auto library = OpenLibrary();
    const vk::Instance instance = CreateInstance(*library, dld, VK_API_VERSION_1_1, wsi.type);
    const std::vector<VkPhysicalDevice> physical_devices = instance.EnumeratePhysicalDevices();
    vk::SurfaceKHR surface = CreateSurface(instance, wsi);

    records.clear();
    records.reserve(physical_devices.size());
    for (const VkPhysicalDevice device : physical_devices) {
        const auto physical_device = vk::PhysicalDevice(device, dld);
        std::string name = physical_device.GetProperties().deviceName;

        const std::vector<VkPresentModeKHR> present_modes =
            physical_device.GetSurfacePresentModesKHR(*surface);

        VkPhysicalDeviceDriverProperties driver_properties{};
        driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        driver_properties.pNext = nullptr;
        VkPhysicalDeviceProperties2 properties{};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
        properties.pNext = &driver_properties;
        dld.vkGetPhysicalDeviceProperties2(physical_device, &properties);

        const auto driverID = driver_properties.driverID;

        bool has_broken_compute{
            Vulkan::Device::CheckBrokenCompute(driverID, properties.properties.driverVersion)};

        std::string driver_string = Vulkan::vk::GetDriverName(driver_properties);

        if (driver_string.empty())
            driver_string = "Unknown";

        const std::string device_name = name;
        name = fmt::format("{} ({})", name, driver_string);

        // Satronho NX: the largest device-local heap is the card's video memory.
        u64 device_local_bytes = 0;
        const VkPhysicalDeviceMemoryProperties2 memory = physical_device.GetMemoryProperties();
        for (u32 i = 0; i < memory.memoryProperties.memoryHeapCount; ++i) {
            const VkMemoryHeap& heap = memory.memoryProperties.memoryHeaps[i];
            if ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
                device_local_bytes = (std::max)(device_local_bytes, u64{heap.size});
            }
        }

        records.push_back(VkDeviceInfo::Record(name, present_modes, has_broken_compute,
                                               device_name, properties.properties.vendorID,
                                               device_local_bytes));
    }
} catch (const Vulkan::vk::Exception& exception) {
    LOG_ERROR(Frontend, "Failed to enumerate devices with error: {}", exception.what());
}

} // namespace VkDeviceInfo
