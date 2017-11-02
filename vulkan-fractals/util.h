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
void check(bool condition, T &&message)
{
    if (condition) return;
    std::cout << "ERROR: " << std::forward<T>(message);
    exit(10);
}

namespace details {

    template<class T>
    static void check_internal(T &&message)
    {
        std::cout << std::forward<T>(message);
    }

    template<class T, class... TRest>
    void check_internal(T &&message, TRest &&...rest)
    {
        std::cout << std::forward<T>(message);
        check_internal(std::forward<TRest>(rest)...);
    }

} // namespace details

template<class T, class... TRest>
void check(bool condition, T &&message, TRest &&...rest)
{
    if (condition) return;
    std::cout << "ERROR: ";
    details::check_internal(std::forward<T>(message), std::forward<TRest>(rest)...);
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

/**
 * Eliminate the need to call a Vulkan listing function twice to allocate a vector by hand.
 * @tparam T Vulkan type to be stored
 * @tparam TPfn Type of function to be called
 * @tparam TParams Types of custom parameters
 * @param pfn Vulkan listing function to be called
 * @param params Params passed to the Vulkan function. Excludes the allocator, count and storing vector as parameters.
 * @return Vector with stored Vulkan objects
 */
template<class T, class TPfn, class... TParams>
std::vector<T> listVulkan(TPfn &&pfn, TParams &&...params)
{
    uint32_t count;
    (*std::forward<TPfn>(pfn))(std::forward<TParams>(params)..., &count, nullptr);
    std::vector<T> vector{count};
    (*std::forward<TPfn>(pfn))(std::forward<TParams>(params)..., &count, vector.data());
    return std::move(vector);
};

/**
 * Check the result to be {@code VK_TRUE}, otherwise print the given error message
 * together with the Vulkan error code string and exit the program.
 * @tparam TParams Types of the message parameters
 * @param result Vulkan result to be checked
 * @param message Message to be printed
 * @param params Message parameters to be printed
 */
template<class... TParams>
void checkVk(VkResult result, const std::string &message, TParams &&...params)
{
    check(result == VK_SUCCESS, '[', vulkanErrorString(result), "] ", message, std::forward<TParams>(params)...);
}

template<class TPfn, class... TParams>
auto invokeVk(const std::string &name, VkInstance instance, TParams &&...params)
{
    auto func = reinterpret_cast<TPfn>(vkGetInstanceProcAddr(instance, name.c_str()));
    check(func != nullptr, "Cannot load Vulkan function ", name);
    return func(instance, std::forward<TParams>(params)...);
};
