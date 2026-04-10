cmake_minimum_required(VERSION 3.25)
project(vulkan-renderer VERSION 0.1.0 LANGUAGES CXX)

# ── C++20 ────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Source files ─────────────────────────────────────────────────────────────
set(SOURCES
        src/main.cpp
        src/VulkanContext.cpp
        src/SwapChain.cpp
        src/Pipeline.cpp
        src/Renderer.cpp
        src/Mesh.cpp
        src/Material.cpp
        src/GaussianSplat.cpp
)

add_executable(vulkan-renderer ${SOURCES})

# ── vcpkg dependencies ───────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(tinyply CONFIG REQUIRED)

# ── Include directories ──────────────────────────────────────────────────────
target_include_directories(vulkan-renderer PRIVATE
        src/
        ${Stb_INCLUDE_DIR}
)

# ── Link libraries ───────────────────────────────────────────────────────────
target_link_libraries(vulkan-renderer PRIVATE
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        glfw
        glm::glm
        GPUOpen::VulkanMemoryAllocator
        tinyobjloader::tinyobjloader
        imgui::imgui
        spdlog::spdlog
        tinyply
)

# ── Platform compile definitions ─────────────────────────────────────────────
if(WIN32)
    target_compile_definitions(vulkan-renderer PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            VK_USE_PLATFORM_WIN32_KHR
    )
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(vulkan-renderer PRIVATE
            VK_USE_PLATFORM_XLIB_KHR
    )
endif()

# ── GLM configuration ────────────────────────────────────────────────────────
# GLM_FORCE_DEPTH_ZERO_TO_ONE is critical - Vulkan depth range is [0,1] not [-1,1].
# Without this your projection matrix will produce wrong depth values.
target_compile_definitions(vulkan-renderer PRIVATE
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_ENABLE_EXPERIMENTAL
)

# ── Warning flags ────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(vulkan-renderer PRIVATE
            /W4
            /WX
            /wd4100   # unreferenced formal parameter
            /wd4201   # nameless struct/union
    )
else()
    target_compile_options(vulkan-renderer PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
    )
endif()

# ── SPIR-V shader compilation (glslc) ────────────────────────────────────────
# Finds glslc from VULKAN_SDK/bin first, then falls back to PATH.
# If not found, shaders must be compiled manually - the warning tells you the command.
# Search order:
# 1. vcpkg installed tools (shaderc package) — works without Vulkan SDK
# 2. VULKAN_SDK bin — works if SDK is installed system-wide
# 3. System PATH fallback
find_program(GLSLC glslc
        HINTS
        "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/shaderc"
        "${CMAKE_SOURCE_DIR}/build/${PRESET_NAME}/vcpkg_installed/x64-windows/tools/shaderc"
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{VULKAN_SDK}/Bin"
        PATHS
        "$ENV{PATH}"
)

if(GLSLC)
    file(GLOB_RECURSE GLSL_SOURCES
            "${CMAKE_SOURCE_DIR}/shaders/*.vert"
            "${CMAKE_SOURCE_DIR}/shaders/*.frag"
            "${CMAKE_SOURCE_DIR}/shaders/*.comp"
    )

    set(SPV_OUTPUTS "")
    foreach(GLSL_FILE ${GLSL_SOURCES})
        get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
        set(SPV_FILE "${CMAKE_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
        add_custom_command(
                OUTPUT  ${SPV_FILE}
                COMMAND ${GLSLC} ${GLSL_FILE} -o ${SPV_FILE}
                DEPENDS ${GLSL_FILE}
                COMMENT "Compiling shader: ${FILE_NAME}"
        )
        list(APPEND SPV_OUTPUTS ${SPV_FILE})
    endforeach()

    add_custom_target(shaders ALL DEPENDS ${SPV_OUTPUTS})
    add_dependencies(vulkan-renderer shaders)
else()
    message(WARNING
            "glslc not found - shaders will NOT be compiled automatically.\n"
            "Compile manually with:\n"
            "  glslc shaders/triangle.vert -o shaders/triangle.vert.spv\n"
            "  glslc shaders/triangle.frag -o shaders/triangle.frag.spv\n"
            "glslc ships with the Vulkan SDK. Check that VULKAN_SDK is set correctly."
    )
endif()

# ── Copy assets to build output directory ────────────────────────────────────
add_custom_command(TARGET vulkan-renderer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:vulkan-renderer>/assets"
        COMMENT "Copying assets to build directory"
)

# ── Copy shaders to build output directory ───────────────────────────────────
add_custom_command(TARGET vulkan-renderer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:vulkan-renderer>/shaders"
        COMMENT "Copying shaders to build directory"
)

# --- Doxygen integration ---
find_package(Doxygen QUIET)

option(BUILD_DOCS "Build API documentation with Doxygen" ON)

if (BUILD_DOCS AND DOXYGEN_FOUND)
    set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile)

    add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_IN}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
    )
endif()