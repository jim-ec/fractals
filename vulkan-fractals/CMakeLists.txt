cmake_minimum_required(VERSION 3.8)
set(MY_TARGET vulkan-fractals)
project(${MY_TARGET})

add_subdirectory(fmt)
add_subdirectory(glfw-3.2.1)

set(CMAKE_CXX_STANDARD 14)

include_directories(inc)
if (WIN32)
    link_directories(lib-win32)
elseif (UNIX)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lvulkan")
endif ()

set(SOURCE_FILES main.cpp Application.cpp Application.h util.h util.cpp Vertex.cpp Vertex.h Buffer.cpp Buffer.h StagingBuffer.cpp StagingBuffer.h Pipeline.cpp Pipeline.h)
add_executable(${MY_TARGET} ${SOURCE_FILES})

if (WIN32)
    target_link_libraries(${MY_TARGET} vulkan-1 glfw fmt)
elseif (UNIX)
    target_link_libraries(${MY_TARGET} glfw fmt)
endif ()

# GLSL shaders:

set(VULKAN_SDK_VERSION 1.0.61.1)

if(WIN32)
    set(GLSL_VALIDATOR C:/install_files/VulkanSDK/${VULKAN_SDK_VERSION}/Bin32/glslangValidator.exe)
elseif(UNIX)
    set(GLSL_VALIDATOR /home/jim/VulkanSDK/${VULKAN_SDK_VERSION}/x86_64/bin/glslangValidator)
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES shaders/*.frag shaders/*.vert)

foreach (GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(spirv-shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${MY_TARGET} spirv-shaders)

add_custom_command(TARGET ${MY_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${MY_TARGET}>/shaders/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:${MY_TARGET}>/shaders"
        )
