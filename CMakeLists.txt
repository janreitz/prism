cmake_minimum_required(VERSION 3.10)
project(ECS)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set policy CMP0072 to prefer GLVND
if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()

# Add options to control profiling and Tracy separately
option(ENABLE_PROFILING_FLAGS "Enable profiling-friendly build flags" ON)
option(ENABLE_TRACY "Enable Tracy instrumentation" ON)

# Apply profiling flags if enabled
if(ENABLE_PROFILING_FLAGS)
    # Flags to aid in profiling
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG -fno-omit-frame-pointer -ginline-points")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG -fno-omit-frame-pointer -ginline-points")
    
    # Also apply to Release builds for consistency
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -DNDEBUG -fno-omit-frame-pointer -ginline-points")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -g -DNDEBUG -fno-omit-frame-pointer -ginline-points")
    
    message(STATUS "Profiling-friendly build flags enabled")
endif()

# Default to RelWithDebInfo
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

if (ENABLE_TRACY)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG        v0.11.1
    )
    FetchContent_MakeAvailable(tracy)
    message(STATUS "MAIN: defining TRACY_ENABLE")
    add_definitions(-DTRACY_ENABLE)
endif()

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG         v1.14.1
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

find_package(OpenGL REQUIRED)

# Include directories
include_directories(
    src
    third_party/glfw/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    third_party/glad/include
    third_party/glm-master
    third_party/stb
    ${ASSIMP_INCLUDE_DIRS}
)

# GLFW Setup
add_subdirectory(third_party/glfw-3.4)

# GLAD Setup
add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# ImGui Setup
file(GLOB IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/*.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Main executable
add_executable(prism 
    src/main.cpp 
    src/filesystem_node.cpp
    ${IMGUI_SOURCES} 
)

target_link_libraries(prism
    PUBLIC
        glad
        glfw
        OpenGL::GL
        spdlog::spdlog
)

# Enable testing
enable_testing()

# Test executable
add_executable(layout_tests
    tests/test_layout.cpp
    src/filesystem_node.cpp
)

target_link_libraries(layout_tests
    PRIVATE
        Catch2::Catch2WithMain
        spdlog::spdlog
)

# Register test with CTest
include(CTest)
include(Catch)
catch_discover_tests(layout_tests)
