#pragma once

#include <iostream>
#include <tuple>
#include <algorithm>
#include <iterator>
#include <array>
#include <utility>

#include <vulkan/vulkan.h>

#include <fmt/printf.h>
#include <fmt/format.h>
#include <fmt/string.h>
#include <fmt/ostream.h>
#include <fmt/time.h>
#include <fmt/container.h>

extern const std::array<std::pair<VkResult, const char *>, 18> VULKAN_ERROR_STRINGS;

template<class T>
void check(bool condition, const T &message)
{
    if (condition) return;
    std::cout << "ERROR: " << message;
    exit(10);
}

namespace details {

    template<class T>
    static void check_internal(const T &message)
    {
        std::cout << message;
    }

    template<class T, class... REST>
    void check_internal(const T &message, const REST &...rest)
    {
        std::cout << message;
        check_internal(rest...);
    }

} // namespace details

template<class T, class... REST>
void check(bool condition, const T &message, const REST &...rest)
{
    if (condition) return;
    std::cout << "ERROR: ";
    details::check_internal(message, rest...);
    exit(10);
}

inline const char *vulkanErrorString(VkResult result)
{
    auto iter = std::find_if(std::begin(VULKAN_ERROR_STRINGS), std::end(VULKAN_ERROR_STRINGS), [&](const auto &map) {
        return map.first == result;
    });
    if (iter == std::end(VULKAN_ERROR_STRINGS)) {
        return "unknown-error";
    }
    return iter->second;
}

template<class T, class PFN, class... PARAMS>
std::vector<T> listVk(const PFN &pfn, const PARAMS &...params)
{
    uint32_t count;
    (*pfn)(params..., &count, nullptr);
    std::vector<T> vector{count};
    (*pfn)(params..., &count, vector.data());
    return std::move(vector);
};

template<class... Args>
void checkVk(VkResult result, const std::string &message, const Args &...args)
{
    check(result == VK_SUCCESS, '[', vulkanErrorString(result), "] ", message, args...);
}

template<class PFN_TYPE, class... Args>
auto invokeVk(const std::string &name, VkInstance instance, const Args &...args)
{
    auto func = reinterpret_cast<PFN_TYPE>(vkGetInstanceProcAddr(instance, name.c_str()));
    check(func != nullptr, "Cannot load Vulkan function ", name);
    return func(instance, args...);
};
