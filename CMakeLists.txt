cmake_minimum_required(VERSION 3.15)

# Enable BOTH C and C++
project(SDL3_ImGui LANGUAGES C CXX)

# -----------------------------
# C / C++ Standards
# -----------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------
# SDL3 paths
# -----------------------------
set(SDL3_DIR ${CMAKE_SOURCE_DIR}/external/SDL3)
set(SDL3_LIB_DIR ${SDL3_DIR}/lib/x64)

# -----------------------------
# Includes
# -----------------------------
include_directories(
    ${SDL3_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/sqLite
    ${CMAKE_SOURCE_DIR}/src/Miscellaneous
    ${CMAKE_SOURCE_DIR}/src/PlatformSpecific/IntegratedPlatformChecks
    ${CMAKE_SOURCE_DIR}/src/GUI
    ${CMAKE_SOURCE_DIR}/src/FontHeader
    ${CMAKE_SOURCE_DIR}/src/Data
    # IMGUI
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
)

# -----------------------------
# ImGui sources (C++)
# -----------------------------
set(IMGUI_SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_sdl3.cpp
    external/imgui/backends/imgui_impl_sdlrenderer3.cpp
)

# -----------------------------
# Project sources
# -----------------------------
file(GLOB PROJECT_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
    src/Data/*.cpp
    src/GUI/*.cpp
    src/Miscellaneous/*.cpp
    src/PlatformSpecific/IntegratedPlatformChecks/*.cpp
)

# -----------------------------
# C sources (SQLite)
# -----------------------------
file(GLOB SQLITE_SOURCES
    CONFIGURE_DEPENDS
    external/sqLite/*.c
)

# -----------------------------
# App
# -----------------------------
add_executable(HPR
    ${PROJECT_SOURCES}
    ${IMGUI_SOURCES}
    ${SQLITE_SOURCES}
)

# -----------------------------
# Detect the platform and link SDL3
# -----------------------------
if (WIN32)
    # Windows-specific code: link SDL3.lib
    set(SDL3_LIBRARY "${SDL3_LIB_DIR}/SDL3.lib")
elseif (UNIX)
    # Linux or other Unix-based systems: find system-wide SDL3
    find_package(SDL3 REQUIRED)
    set(SDL3_LIBRARY SDL3::SDL3)  # CMake target for SDL3
endif()

# Link SDL3 library
target_link_libraries(HPR
    ${SDL3_LIBRARY}
)

# -----------------------------
# AUTO COPY SDL3.dll (only for Windows)
# -----------------------------
if (WIN32)
    add_custom_command(
        TARGET HPR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL3_LIB_DIR}/SDL3.dll"
            $<TARGET_FILE_DIR:HPR>
    )
endif()
