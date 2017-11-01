#include "util.h"

const std::array<std::pair<VkResult, const char *>, 18> VULKAN_ERROR_STRINGS = {{
        {VK_SUCCESS,   "success"},
        {VK_NOT_READY, "not-ready"},
        {VK_TIMEOUT, "timeout"},
        {VK_EVENT_SET, "event-set"},
        {VK_EVENT_RESET, "event-reset"},
        {VK_INCOMPLETE, "incomplete"},
        {VK_ERROR_OUT_OF_HOST_MEMORY, "out-of-host-memory"},
        {VK_ERROR_OUT_OF_DEVICE_MEMORY, "out-of-device-memory"},
        {VK_ERROR_INITIALIZATION_FAILED, "initialization-failed"},
        {VK_ERROR_DEVICE_LOST, "device-lost"},
        {VK_ERROR_MEMORY_MAP_FAILED, "memory-map-failed"},
        {VK_ERROR_LAYER_NOT_PRESENT, "layer-not-present"},
        {VK_ERROR_EXTENSION_NOT_PRESENT, "extension-not-present"},
        {VK_ERROR_FEATURE_NOT_PRESENT, "feature-not-present"},
        {VK_ERROR_INCOMPATIBLE_DRIVER, "incompatible-driver"},
        {VK_ERROR_TOO_MANY_OBJECTS, "too-many-objects"},
        {VK_ERROR_FORMAT_NOT_SUPPORTED, "format-not-supported"},
        {VK_ERROR_FRAGMENTED_POOL, "fragmented-pool"},
}};
