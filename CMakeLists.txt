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
option(ENABLE_TRACY "Enable Tracy instrumentation" OFF)
option(ENABLE_TESTS "Enable tests" OFF)

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
        GIT_TAG        master
    )
    FetchContent_MakeAvailable(tracy)
    message(STATUS "MAIN: defining TRACY_ENABLE")
    add_definitions(-DTRACY_ENABLE)
endif()

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

find_package(OpenGL REQUIRED)
find_package(TBB QUIET)

# Find LLVM and Clang
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set up LLVM with minimal components
llvm_map_components_to_libnames(llvm_libs support core)

if(TBB_FOUND)
    message(STATUS "TBB found - enabling parallel execution")
    set(ENABLE_PARALLEL_EXECUTION ON)
else()
    message(STATUS "TBB not found - parallel execution disabled")
    set(ENABLE_PARALLEL_EXECUTION OFF)
endif()

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
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
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
    src/treemap.cpp
    src/filesystem_view.cpp
    src/ast_node.cpp
    src/ast_generation.cpp
    src/ast_analysis.cpp
    src/ast_matcher.cpp
    src/ast_matcher_view.cpp
    src/utils.cpp
    ${IMGUI_SOURCES} 
)

target_link_libraries(prism
    PUBLIC
        glad
        glfw
        OpenGL::GL
        ${llvm_libs}
        clangAST
        clangBasic
        clangASTMatchers
        clangTooling
)

foreach(target prism)
    target_compile_features(${target} PRIVATE cxx_std_23)

    if(ENABLE_TRACY)
        target_link_libraries(${target} PUBLIC Tracy::TracyClient)
        target_compile_definitions(${target} PRIVATE TRACY_ENABLE=1)
    else()
        target_compile_definitions(${target} PRIVATE TRACY_ENABLE=0)
    endif()

    if(ENABLE_PARALLEL_EXECUTION)
        target_compile_definitions(${target} PRIVATE ENABLE_PARALLEL_EXECUTION=1)
        target_link_libraries(${target} PRIVATE TBB::tbb)
    else()
        target_compile_definitions(${target} PRIVATE ENABLE_PARALLEL_EXECUTION=0)
    endif()
endforeach()


if (ENABLE_TESTS)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

enable_testing()

# Test executable
add_executable(layout_tests
    tests/test_layout.cpp
    src/filesystem_node.cpp
    src/treemap.cpp
)

target_link_libraries(layout_tests
    PRIVATE
        Catch2::Catch2WithMain
)

# Register test with CTest
include(CTest)
include(Catch)
catch_discover_tests(layout_tests)
endif()

