cmake_minimum_required(VERSION 3.21)

project(HPR)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)

find_package(Slint QUIET)

# Find libdbus-1 on Linu
if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DBUS REQUIRED dbus-1)
endif()



file(GLOB_RECURSE APPSTATE src/appState/*.cpp)
file(GLOB_RECURSE DATABASE src/database/*.cpp)
file(GLOB_RECURSE EXTENSION src/extension/*.cpp)
file(GLOB_RECURSE MISC src/miscellaneous/*.cpp)
file(GLOB_RECURSE PATTERN src/pattern/*.cpp)
file(GLOB_RECURSE TRAY src/tray/*.cpp)
file(GLOB_RECURSE UI src/ui-cpp/*.cpp)
file(GLOB_RECURSE WINDOW src/window/*.cpp)

set(SOURCES
    src/main.cpp
    ${APPSTATE}
    ${DATABASE}
    ${EXTENSION}
    ${MISC}
    ${PATTERN}
    ${TRAY}
    ${UI}
    ${WINDOW}
    external/sqLite/sqlite3.c
)

if(WIN32)
    add_compile_definitions(NOMINMAX)
    list(APPEND SOURCES external/wintoast/wintoastlib.cpp assets/app.rc)
endif()

if(WIN32)
    add_executable(HPR WIN32 ${SOURCES})
elseif(LINUX)
    add_executable(HPR ${SOURCES})
endif()

if (MSVC)
    target_compile_options(HPR PRIVATE /MP)
endif()

# Lua static lib
file(GLOB_RECURSE LUA_SOURCES "external/lua/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*(lua|luac)\\.c$")
add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC external/lua/)
if(LINUX)
    target_compile_definitions(lua_static PRIVATE LUA_USE_LINUX)
endif()

# Link to HPR
target_link_libraries(HPR PRIVATE lua_static)

# sol2 headers
target_include_directories(HPR PRIVATE external/sol2/)

target_link_libraries(HPR PRIVATE Slint::Slint)

if(WIN32)
    target_link_libraries(HPR PRIVATE winhttp ws2_32)
    target_include_directories(HPR PRIVATE external/wintoast/)
elseif(LINUX)
    find_package(CURL REQUIRED)
    target_link_libraries(HPR PRIVATE ${DBUS_LIBRARIES} CURL::libcurl)
    target_include_directories(HPR PRIVATE ${DBUS_INCLUDE_DIRS})
endif()

slint_target_sources(HPR src/ui/app-window.slint)

# Embed resources directly into the application binary instead of relying on absolute paths
set_target_properties(HPR PROPERTIES 
    SLINT_EMBED_RESOURCES embed-files
)

target_include_directories(HPR
    PRIVATE
    include/
    external/sqLite/
    external/sqlite_modern_cpp
    external/sqlite_modern_cpp/sqlite_modern_cpp
    external/sqlite_modern_cpp/sqlite_modern_cpp/lists
    external/sqlite_modern_cpp/sqlite_modern_cpp/utility
)

# On Windows, copy the Slint DLL next to the application binary so that it's found.
if (WIN32)
    add_custom_command(TARGET HPR POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:HPR> $<TARGET_FILE_DIR:HPR> COMMAND_EXPAND_LISTS)
endif()

# Copy some aliases.csv to binary path
add_custom_command(TARGET HPR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/shippedWithBinary/aliases.csv
    $<TARGET_FILE_DIR:HPR>/aliases.csv
)

# Copy some ui to shipped with binary path
file(COPY ${CMAKE_SOURCE_DIR}/src/ui
     DESTINATION ${CMAKE_SOURCE_DIR}/shippedWithBinary)

# Copy some tabAliases.csv to binary path
add_custom_command(TARGET HPR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/shippedWithBinary/tabAliases.csv
    $<TARGET_FILE_DIR:HPR>/tabAliases.csv
)

# Copy some projectAliases.csv to binary path
add_custom_command(TARGET HPR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/shippedWithBinary/projectAliases.csv
    $<TARGET_FILE_DIR:HPR>/projectAliases.csv
)


# Copy config.csv to binary path
add_custom_command(TARGET HPR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/shippedWithBinary/config.csv
    $<TARGET_FILE_DIR:HPR>/config.csv
)

# Copy some config installer to binary path
if(UNIX)
    add_custom_command(TARGET HPR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/shippedWithBinary/installHPRConfigAndUi.sh
        $<TARGET_FILE_DIR:HPR>/installHPRConfigAndUi.sh
    )

else()

    # Copy some config installer to binary path, windows
    add_custom_command(TARGET HPR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/shippedWithBinary/installHPRConfigAndUi.bat
        $<TARGET_FILE_DIR:HPR>/installHPRConfigAndUi.bat
    )
endif()

#copy assets folder next to binary on every build
add_custom_command(TARGET HPR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:HPR>/assets
)

#copy ui folder to binary every build
add_custom_command(TARGET HPR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/ui
    $<TARGET_FILE_DIR:HPR>/ui
)


if(UNIX)
    add_custom_command(TARGET HPR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/shippedWithBinary/installWindowCallsExtension.sh
        $<TARGET_FILE_DIR:HPR>/installWindowCallsExtension.sh
    )

    find_library(SLINT_SO
        NAMES slint_cpp
        HINTS "${Slint_DIR}/../.."
              "${Slint_DIR}/../../.."
    )

    if(SLINT_SO)
        add_custom_command(TARGET HPR POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SLINT_SO}"
                $<TARGET_FILE_DIR:HPR>
        )
    else()
        message(WARNING "[HPR] Could not find libslint_cpp.so — you may need to copy it manually next to the binary")
    endif()

    set_target_properties(HPR PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()