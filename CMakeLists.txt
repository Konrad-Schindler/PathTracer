cmake_minimum_required(VERSION 3.29)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vendor/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

project(Pathtracer)

# config
set(CMAKE_CXX_STANDARD 20)

# vcpkg
set(VCPKG_MANIFEST_MODE ON)

# bootstrap vcpkg if missing
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/vendor/vcpkg/vcpkg.exe)
    message(STATUS "Bootstrapping vcpkg...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_SOURCE_DIR}/vendor/vcpkg" ./bootstrap-vcpkg.bat
    )
endif()

# dependencies
find_package(Vulkan REQUIRED COMPONENTS glslc)

find_package(glfw3 CONFIG REQUIRED)

add_subdirectory(vendor/vk-bootstrap)

# compile shaders
find_program(GLSLC glslc)
if(NOT GLSLC) 
    message(FATAL_ERROR "glslc not found.")
endif()

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
    "shaders/*.vert"
    "shaders/*.frag"
    "shaders/*.comp"
    "shaders/*.geom"
    "shaders/*.tesc"
    "shaders/*.tese"
)

message("Compiling shaders")
set(SHADER_BINARIES)
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME_WLE)
    get_filename_component(EXT ${SHADER} LAST_EXT)
    string(SUBSTRING ${EXT} 1 -1  STAGE)

    set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}_${STAGE}.spv")
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND ${GLSLC} ${SHADER} -o ${OUTPUT_FILE}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER}..." 
    )
    list(APPEND SHADER_BINARIES ${OUTPUT_FILE})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SHADER_BINARIES})

set(SOURCES 
    src/main.cpp 
    src/engine.hpp 
    src/engine.cpp 
    src/vk_images.hpp 
    src/vk_images.cpp 
    src/vk_initializers.hpp 
    src/vk_initializers.cpp 
    src/vk_types.hpp
    src/vk_descriptors.hpp
    src/vk_descriptors.cpp
    src/vk_pipelines.hpp
    src/vk_pipelines.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
add_dependencies(${PROJECT_NAME} compile_shaders)

target_include_directories(${PROJECT_NAME} PRIVATE
    vendor/vk_mem_alloc
    ${CMAKE_CURRENT_BINARY_DIR}/shaders
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan
    glfw
    vk-bootstrap
)