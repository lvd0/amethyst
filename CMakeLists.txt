cmake_minimum_required(VERSION 3.22)
project(amethyst)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
    # TODO: Defines for Linux and MacOS
endif()

add_subdirectory(ext/volk)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/glfw)

set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(ext/SPIRV-Cross)

set(ENKITS_BUILD_C_INTERFACE OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/enkiTS)

set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(ext/SPIRV-Headers)
add_subdirectory(ext/SPIRV-Tools)
add_subdirectory(ext/glslang)

set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(ext/shaderc)

set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VULKAN OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/KTX-Software)

set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "" FORCE)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/VulkanMemoryAllocator)

add_subdirectory(ext/meshoptimizer)

add_library(imgui
    ext/imgui/imconfig.h
    ext/imgui/imgui.h
    ext/imgui/imgui.cpp
    ext/imgui/imgui_demo.cpp
    ext/imgui/imgui_draw.cpp
    ext/imgui/imgui_internal.h
    ext/imgui/imgui_tables.cpp
    ext/imgui/imgui_widgets.cpp
    ext/imgui/imstb_rectpack.h
    ext/imgui/imstb_textedit.h
    ext/imgui/imstb_truetype.h
    ext/imgui/backends/imgui_impl_glfw.h
    ext/imgui/backends/imgui_impl_glfw.cpp
    ext/imgui/backends/imgui_impl_vulkan.h
    ext/imgui/backends/imgui_impl_vulkan.cpp)
target_include_directories(imgui PUBLIC ext/imgui)
target_link_libraries(imgui PUBLIC glfw volk)
target_compile_definitions(imgui PUBLIC
    VK_ENABLE_BETA_EXTENSIONS
    VK_NO_PROTOTYPES)

add_library(implot
    ext/implot/implot.cpp
    ext/implot/implot.h
    ext/implot/implot_demo.cpp
    ext/implot/implot_internal.h
    ext/implot/implot_items.cpp)
target_include_directories(implot PUBLIC
    ext/imgui
    ext/implot)

add_library(vma STATIC src/ext/vma.cpp)
target_include_directories(vma PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ext/VulkanMemoryAllocator/include)
target_link_libraries(vma PUBLIC volk)

add_library(cgltf STATIC
    ext/cgltf/cgltf.h
    src/ext/cgltf.c)
target_include_directories(cgltf PUBLIC ext/cgltf)

add_subdirectory(ext/glm)

set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(ext/spdlog)

target_compile_options(spdlog PRIVATE $<$<BOOL:${MSVC}>:/w>)
target_compile_definitions(spdlog PRIVATE FMT_EXCEPTIONS=0)

# Options
option(AMETHYST_ENABLE_TRACY "Builds and links against Tracy to profile the application." OFF)
option(AMETHYST_ENABLE_AFTERMATH "Uses NVIDIA NSight Aftermath SDK to debug Vulkan failures." OFF)
option(AMETHYST_ENABLE_LOGGING "Enables logger output to stdout." ON)
option(AMETHYST_CUDA_COMPATIBILITY "Makes the library compatible with nvcc." OFF)
option(AMETHYST_BUILD_SHARED "Builds the application as a shared library." OFF)
option(AMETHYST_BUILD_TESTS "Builds the application's test programs." OFF)

set(AMETHYST_ENABLE_TRACY OFF CACHE BOOL "" FORCE)
set(AMETHYST_ENABLE_AFTERMATH OFF CACHE BOOL "" FORCE)
set(AMETHYST_ENABLE_LOGGING ON CACHE BOOL "" FORCE)
set(AMETHYST_CUDA_COMPATIBILITY OFF CACHE BOOL "" FORCE)
set(AMETHYST_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(AMETHYST_BUILD_TESTS ON CACHE BOOL "" FORCE)

if (AMETHYST_ENABLE_AFTERMATH)
    set(NSIGHT_AFTERMATH_INCLUDE_DIRS $ENV{NSIGHT_AFTERMATH_SDK}/include)
    set(NSIGHT_AFTERMATH_LIBRARY_DIRS $ENV{NSIGHT_AFTERMATH_SDK}/lib/x64)
endif()

include(FetchContent)

if (AMETHYST_ENABLE_TRACY)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG a8511d357650282c6915f7aa9775e9c4945e42b5
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(tracy)
endif()

add_library(amethyst
    ### Headers
    # Core
    include/amethyst/core/expected.hpp
    include/amethyst/core/file_view.hpp
    include/amethyst/core/rc_ptr.hpp
    include/amethyst/core/ref_counted.hpp

    # Graphics
    include/amethyst/graphics/async_mesh.hpp
    include/amethyst/graphics/async_model.hpp
    include/amethyst/graphics/async_texture.hpp
    include/amethyst/graphics/virtual_allocator.hpp
    include/amethyst/graphics/clear_value.hpp
    include/amethyst/graphics/command_buffer.hpp
    include/amethyst/graphics/context.hpp
    include/amethyst/graphics/descriptor_pool.hpp
    include/amethyst/graphics/descriptor_set.hpp
    include/amethyst/graphics/device.hpp
    include/amethyst/graphics/fence.hpp
    include/amethyst/graphics/framebuffer.hpp
    include/amethyst/graphics/image.hpp
    include/amethyst/graphics/pipeline.hpp
    include/amethyst/graphics/query_pool.hpp
    include/amethyst/graphics/queue.hpp
    include/amethyst/graphics/render_pass.hpp
    include/amethyst/graphics/semaphore.hpp
    include/amethyst/graphics/swapchain.hpp
    include/amethyst/graphics/typed_buffer.hpp
    include/amethyst/graphics/ui_context.hpp

    # Meta
    include/amethyst/meta/constants.hpp
    include/amethyst/meta/debug_marker.hpp
    include/amethyst/meta/enums.hpp
    include/amethyst/meta/forwards.hpp
    include/amethyst/meta/hash.hpp
    include/amethyst/meta/macros.hpp
    include/amethyst/meta/types.hpp

    # Window
    include/amethyst/window/input.hpp
    include/amethyst/window/window.hpp
    include/amethyst/window/windowing_system.hpp

    ### Sources
    # Core
    src/core/file_view.cpp
    src/core/ref_counted.cpp

    # Graphics
    src/graphics/async_mesh.cpp
    src/graphics/async_model.cpp
    src/graphics/async_texture.cpp
    src/graphics/virtual_allocator.cpp
    src/graphics/command_buffer.cpp
    src/graphics/context.cpp
    src/graphics/descriptor_pool.cpp
    src/graphics/descriptor_set.cpp
    src/graphics/device.cpp
    src/graphics/fence.cpp
    src/graphics/framebuffer.cpp
    src/graphics/image.cpp
    src/graphics/pipeline.cpp
    src/graphics/query_pool.cpp
    src/graphics/queue.cpp
    src/graphics/render_pass.cpp
    src/graphics/semaphore.cpp
    src/graphics/swapchain.cpp
    src/graphics/ui_context.cpp

    # Meta

    # Window
    src/window/input.cpp
    src/window/window.cpp
    src/window/windowing_system.cpp)

target_compile_definitions(shaderc_util PUBLIC
    $<$<BOOL:${WIN32}>:
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX>)

target_compile_definitions(amethyst PUBLIC
    $<$<CONFIG:Debug>:AM_DEBUG>
    $<$<BOOL:${AMETHYST_ENABLE_TRACY}>:AM_ENABLE_PROFILING>
    $<$<BOOL:${AMETHYST_ENABLE_AFTERMATH}>:AM_ENABLE_AFTERMATH>

    $<$<BOOL:${WIN32}>:
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX>

    $<$<BOOL:${WIN32}>:AM_PLATFORM_WIN32>
    $<$<BOOL:${LINUX}>:AM_PLATFORM_LINUX>

    $<$<BOOL:${AMETHYST_BUILD_SHARED}>:AM_BUILD_DLL>
    $<$<BOOL:${AMETHYST_ENABLE_LOGGING}>:AM_DEBUG_LOGGING>
    $<$<BOOL:${AMETHYST_CUDA_COMPATIBILITY}>:AM_CUDA_COMPAT>

    SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS

    VK_ENABLE_BETA_EXTENSIONS
    VK_NO_PROTOTYPES

    TRACY_ENABLE

    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS)

target_compile_options(amethyst PRIVATE
    $<IF:$<BOOL:${MSVC}>,/W4,-Wall -Wextra -pedantic>
    $<$<NOT:$<BOOL:${WIN32}>>:-march=native>)

target_include_directories(amethyst PUBLIC
    include
    data/shaders
    ${NSIGHT_AFTERMATH_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS})

target_link_directories(amethyst PUBLIC
    ${NSIGHT_AFTERMATH_LIBRARY_DIRS})

target_link_libraries(amethyst PUBLIC
    volk
    ktx
    glm
    vma
    glfw
    cgltf
    imgui
    implot
    spdlog
    enkiTS
    shaderc
    meshoptimizer
    spirv-cross-glsl
    $<$<BOOL:${AMETHYST_ENABLE_AFTERMATH}>:GFSDK_Aftermath_Lib.x64>
    $<$<BOOL:${AMETHYST_ENABLE_TRACY}>:TracyClient>)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data)

add_custom_command(TARGET amethyst PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/data/fonts
    ${CMAKE_BINARY_DIR}/data/fonts)

if (AMETHYST_ENABLE_AFTERMATH)
    configure_file(
        $ENV{NSIGHT_AFTERMATH_SDK}/lib/x64/GFSDK_Aftermath_Lib.x64.dll
        ${CMAKE_CURRENT_BINARY_DIR}/GFSDK_Aftermath_Lib.x64.dll
        COPYONLY)
endif()

if (AMETHYST_BUILD_TESTS)
    add_executable(test_shadows tests/test_shadows.cpp)
    target_link_libraries(test_shadows PRIVATE amethyst)
endif()